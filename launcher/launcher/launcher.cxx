#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <tuple>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/process.hpp>

#include <launcher/launcher-cache.hxx>
#include <launcher/launcher-download.hxx>
#include <launcher/launcher-github.hxx>
#include <launcher/launcher-http.hxx>
#include <launcher/launcher-manifest.hxx>
#include <launcher/launcher-options.hxx>
#include <launcher/launcher-progress.hxx>
#include <launcher/launcher-steam.hxx>
#include <launcher/launcher-update.hxx>

#ifdef __linux__
#  include <launcher/launcher-steam-proton.hxx>
#endif

#include <launcher/version.hxx>

#ifdef _WIN32
#  include <windows.h>
#endif

// Include miniz last.
//
// We do this to picks up any configuration macros (like _FILE_OFFSET_BITS=64)
// that might be defined in our project headers or by the build system.
//
#include <miniz.h>

using namespace std;
namespace fs = filesystem;
namespace asio = boost::asio;

using namespace boost::asio::experimental::awaitable_operators;

namespace launcher
{
  // Generate a collision-resistant identifier for filesystem paths.
  //
  // We need to associate metadata (like the "accepted" state) with specific
  // installation paths. Since paths can contain characters that are invalid for
  // filenames (separators, etc.) or exceed length limits, hashing the string
  // gives us a stable, safe identifier.
  //
  static string
  path_digest (const fs::path& p)
  {
    std::hash<string> h;
    return std::to_string (h (p.string ()));
  }

  // Determine the directory for user preference and state caching.
  //
  // We try to be good citizens by respecting platform conventions (XDG on
  // Linux, AppData on Windows). However, if we can't create directories there
  // (e.g., read-only home, restricted environment), we fall back to a local
  // ".launcher-cache" in the current working directory to avoid crashing.
  //
  static fs::path
  resolve_cache_root (const fs::path& scope = {})
  {
    fs::path d;

#ifdef _WIN32
    // On Windows, caches technically belong in LocalAppData.
    //
    if (const char* v = getenv ("LOCALAPPDATA"))
      d = fs::path (v) / "iw4x";
    else if (const char* v = getenv ("APPDATA"))
      d = fs::path (v) / "iw4x";
    else
      d = fs::current_path () / ".iw4x";
#elif defined(__APPLE__)
    if (const char* h = getenv ("HOME"))
      d = fs::path (h) / "Library" / "Application Support" / "iw4x";
    else
      d = fs::current_path () / ".iw4x";
#else
    // On Linux/Unix, we respect the XDG Base Directory specification.
    //
    if (const char* v = getenv ("XDG_CACHE_HOME"))
      d = fs::path (v) / "iw4x";
    else if (const char* h = getenv ("HOME"))
      d = fs::path (h) / ".cache" / "iw4x";
    else
      d = fs::current_path () / ".iw4x";
#endif

    // If we are looking for the cache specific to an installation (to avoid
    // conflicts between multiple installs), append its unique key.
    //
    if (!scope.empty ())
      d /= path_digest (scope);

    error_code ec;
    fs::create_directories (d, ec);

    if (ec)
    {
      // Fallback: If we can't write to the system location, try a local
      // directory.
      //
      d = fs::current_path () / ".iw4x";

      if (!scope.empty ())
        d /= path_digest (scope);

      fs::create_directories (d, ec);
    }

    return d;
  }

  // Aggregates all the configuration options, environment paths, and flags
  // derived from CLI arguments and platform detection, so we can pass them
  // around as a single unit.
  //
  struct runtime_context
  {
    fs::path       install_location;
    string         upstream_owner;
    string         upstream_repo;
    bool           prerelease;
    size_t         concurrency_limit;
    fs::path       proton_binary;
    vector<string> proton_arguments;
  };

  // Aggregates remote state required for synchronization.
  //
  struct remote_state
  {
    github_release client;
    github_release raw;
    github_release helper;
    string dlc_manifest_json;
  };

  // Main controller for the bootstrap process.
  //
  class launcher_controller
  {
  public:
    launcher_controller (asio::io_context& ioc, runtime_context ctx)
      : ioc_ (ioc),
        ctx_ (move (ctx)),
        github_ (ioc_),
        http_ (ioc_),
        downloads_ (ioc_, ctx_.concurrency_limit),
        progress_ (ioc_),
        cache_ (ioc_, ctx_.install_location)
    {
      github_.set_progress_callback (
        [this] (const string& message, uint64_t seconds_remaining)
      {
        this->handle_rate_limit_progress (message, seconds_remaining);
      });

      // Wire cache coordinator to other subsystems.
      //
      cache_.set_github_coordinator (&github_);
      cache_.set_download_coordinator (&downloads_);
      cache_.set_progress_coordinator (&progress_);
    }

    asio::awaitable<int>
    run ()
    {
      remote_state remote (co_await resolve_remote_state ());

      co_await reconcile_artifacts (remote);
      co_return co_await execute_payload ();
    }

  private:
    // Fetch upstream metadata.
    //
    // We use make_parallel_group to launch requests concurrently. This allows
    // us to capture exceptions from individual tasks and rethrow them after
    // the group completes.
    //
    // Note that we force pre-release semantics for the steam helper (passed
    // as 'true') because it is strictly a beta component.
    //
    asio::awaitable<remote_state>
    resolve_remote_state ()
    {
      using namespace asio::experimental;

      // Note: The destructuring assignment below is intentionally verbose and
      // admittedly ugly.
      //
      // asio::experimental::make_parallel_group() returns a tuple that encodes
      // *both* the completion order and the result of each operation as a pair
      // of (exception_ptr, value). This design allows all operations to run to
      // completion and lets the caller decide how to handle partial failures,
      // rather than failing fast on the first error.
      //
      // Because of that, we must explicitly bind each exception/result pair in
      // a fixed order, even though only the results are ultimately used.
      //
#ifdef __linux__
      auto [ord, ex_c, c, ex_r, r, ex_h, h, ex_d, d] =
#else
      auto [ord, ex_c, c, ex_r, r, ex_d, d] =
#endif
        co_await make_parallel_group (
          asio::co_spawn (
            ioc_,
            github_.fetch_latest_release (ctx_.upstream_owner,
                                          ctx_.upstream_repo,
                                          ctx_.prerelease),
            asio::deferred),
          asio::co_spawn (
            ioc_,
            github_.fetch_latest_release ("iw4x",
                                          "iw4x-rawfiles",
                                          ctx_.prerelease),
            asio::deferred),
#ifdef __linux__
          asio::co_spawn (
            ioc_,
            github_.fetch_latest_release ("iw4x",
                                          "launcher-steam",
                                          true),
            asio::deferred),
#endif
          asio::co_spawn (
            ioc_,
            [this] () -> asio::awaitable<string>
            {
              co_return co_await http_.get ("https://cdn.iw4x.io/update.json");
            }(),
            asio::deferred)
        ).async_wait (wait_for_all (), asio::use_awaitable);

      if (ex_c) rethrow_exception (ex_c);
      if (ex_r) rethrow_exception (ex_r);
#ifdef __linux__
      if (ex_h) rethrow_exception (ex_h);
#endif
      if (ex_d) rethrow_exception (ex_d);

#ifdef __linux__
      co_return remote_state {move (c), move (r), move (h), move (d)};
#else
      co_return remote_state {move (c), move (r), {}, move (d)};
#endif
    }

    asio::awaitable<void>
    reconcile_artifacts (const remote_state& r)
    {
      using ct = component_type;
      using fs_st = file_state;
      using ga = github_asset;
      using ma = manifest_archive;
      using sv = string_view;

      // First, check the high-level state. If our tags match the remote tags,
      // we are theoretically up to date, but we must also audit the files on
      // disk in case the user hasn't manually deleted or corrupted something.
      //
      bool c_out (cache_.outdated (ct::client, r.client.tag_name));
      bool r_out (cache_.outdated (ct::rawfiles, r.raw.tag_name));

    #ifdef __linux__
      bool h_out (cache_.outdated (ct::helper, r.helper.tag_name));
    #else
      bool h_out (false);
    #endif

      if (!c_out && !r_out && !h_out)
      {
        auto valid ([this] (ct t)
        {
          auto s (cache_.audit (t));
          return std::all_of (s.begin (), s.end (), [] (const auto& p)
          {
            return p.second == fs_st::valid;
          });
        });

        // If tags match and physical files are valid, we can short-circuit.
        //
        if (valid (ct::client) &&
            valid (ct::rawfiles)
    #ifdef __linux__
            && valid (ct::helper)
    #endif
        ) co_return;
      }

      // We need to merge the 'raw' assets and 'client' assets into a single
      // operational manifest. To do this, we fetch both simultaneously.
      //
      auto t_m (github_.fetch_manifest (r.client));
      auto t_r (github_.fetch_manifest (r.raw));

      auto [d_m, d_r] =
        co_await (std::move (t_m) && std::move (t_r));

      manifest m (std::move (d_m));
      manifest raw (std::move (d_r));

      // The raw manifest doesn't map 1:1 to the download structure. We index
      // all known hashes from both sources so we can attach integrity data to
      // the assets we are about to inject.
      //
      unordered_map<string, manifest::hash_type> hashes;
      hashes.reserve (m.files.size () + raw.files.size ());

      auto idx ([&hashes] (const manifest& x)
      {
        for (const auto& a : x.archives)
          if (!a.name.empty () && !a.hash.value.empty ())
            hashes[a.name] = a.hash;

        for (const auto& f : x.files)
        {
          if (f.path.empty () || f.hash.value.empty ())
            continue;

          hashes[f.path] = f.hash;
          hashes[fs::path (f.path).filename ().string ()] = f.hash;
        }
      });

      idx (m);
      idx (raw);

      // We need quick lookups for raw structure to map assets to their
      // internal metadata.
      //
      unordered_map<sv, const ma*> raw_archs;
      for (const auto& a : raw.archives) raw_archs[a.name] = &a;

      unordered_map<string, const manifest_file*> raw_files;
      for (const auto& f : raw.files)
      {
        if (f.archive_name) continue;

        if (f.asset_name) raw_files[*f.asset_name] = &f;
        else raw_files[fs::path (f.path).filename ().string ()] = &f;
      }

      unordered_map<sv, const ga*> c_assets;
      for (const auto& a : r.client.assets) c_assets[a.name] = &a;

      // Consolidate injection logic. We iterate the github assets and convert
      // them into manifest archives, trying to resolve their hash from the
      // indices we built above.
      //
      auto inject ([&] (const vector<ga>& assets, const string& filter = "")
      {
        for (const auto& a : assets)
        {
          if (!filter.empty () && a.name != filter) continue;

          ma x;
          x.name = a.name;
          x.url = a.browser_download_url;
          x.size = a.size;

          if (auto it (raw_archs.find (a.name)); it != raw_archs.end ())
          {
            x.hash = it->second->hash;
            x.files = it->second->files;
          }
          else if (auto it (raw_files.find (a.name)); it != raw_files.end ())
          {
            x.hash = it->second->hash;
          }
          else if (auto it (hashes.find (a.name)); it != hashes.end ())
          {
            x.hash = it->second;
          }

          m.archives.push_back (std::move (x));
        }
      });

      m.archives.reserve (m.archives.size () + r.raw.assets.size ());
      inject (r.raw.assets);

    #ifdef __linux__
      // On Linux, we need the steam helper binaries which live in a
      // separate repo.
      //
      inject (r.helper.assets, "steam.exe");
      inject (r.helper.assets, "steam_api64.dll");
    #endif

      if (!r.dlc_manifest_json.empty ())
      {
        manifest dlc (r.dlc_manifest_json, manifest_format::dlc);
        m.archives.reserve (m.archives.size () + dlc.files.size ());

        // DLCs are treated as archives for download.
        //
        for (const auto& f : dlc.files)
        {
          if (f.path.empty ()) continue;

          ma x;
          x.name = f.path;
          x.url = "https://cdn.iw4x.io/" + f.path;
          x.size = f.size;
          x.hash = f.hash;

          m.archives.push_back (std::move (x));
        }
      }

      // Ask the reconciler what actually needs to be done based on the
      // assembled manifest.
      //
      auto plan (cache_.get_reconciler ().plan (m, ct::client, r.client.tag_name));

      unordered_map<sv, const manifest_file*> m_files;
      for (const auto& f : m.files)
        if (f.asset_name) m_files[*f.asset_name] = &f;

      // The reconciler doesn't know about GitHub URLs, so we patch
      // them back into the plan.
      //
      for (auto& item : plan)
      {
        if (item.action != reconcile_action::download || !item.url.empty ())
          continue;

        fs::path p (item.path);
        string fn (p.filename ().string ());

        if (m_files.count (fn))
        {
          if (auto it (c_assets.find (fn)); it != c_assets.end ())
            item.url = it->second->browser_download_url;
        }
      }

      auto sum (cache_.get_reconciler ().summarize (plan));

      auto stamp ([this, &r] ()
      {
        cache_.stamp (ct::client, r.client.tag_name);
        cache_.stamp (ct::rawfiles, r.raw.tag_name);
    #ifdef __linux__
        cache_.stamp (ct::helper, r.helper.tag_name);
    #endif
      });

      if (sum.up_to_date ())
      {
        stamp ();
        co_return;
      }

      // Execute downloads. We map the active task to its progress entry so we
      // can update the UI and clean up finished tasks in the loop.
      //
      unordered_map<shared_ptr<download_coordinator::task_type>,
                    shared_ptr<progress_entry>> tasks;
      tasks.reserve (plan.size ());

      // Count how many downloads we actually need.
      //
      std::size_t download_count (0);

      for (const auto& item : plan)
      {
        if (item.action != reconcile_action::download || item.url.empty ())
          continue;

        ++download_count;

        fs::path dst (item.path);
        if (dst.has_parent_path ())
        {
          std::error_code ec;
          fs::create_directories (dst.parent_path (), ec);
        }

        download_request req;
        req.urls.push_back (item.url);
        req.target = dst;
        req.name = dst.filename ().string ();
        req.expected_size = item.expected_size;

        string n (req.name);
        auto t (downloads_.queue_download (std::move (req)));
        auto e (progress_.add_entry (n));

        e->metrics ().total_bytes.store (item.expected_size,
                                        std::memory_order_relaxed);
        tasks[t] = e;

        t->on_progress = [e, this] (const download_progress& p)
        {
          progress_.update_progress (e, p.downloaded_bytes, p.total_bytes);
        };
      }

      // Only show the progress UI if there are actual downloads.
      //
      if (download_count > 0)
      {
        progress_.start ();

        asio::co_spawn (ioc_, downloads_.execute_all (), asio::detached);

        // Wait for the queue to drain.
        //
        asio::steady_timer timer (ioc_);
        while (downloads_.completed_count () + downloads_.failed_count () <
              downloads_.total_count ())
        {
          std::erase_if (tasks, [&] (const auto& kv)
          {
            if (kv.first->completed () || kv.first->failed ())
            {
              progress_.remove_entry (kv.second);
              return true;
            }
            return false;
          });

          timer.expires_after (chrono::milliseconds (25));
          co_await timer.async_wait (asio::use_awaitable);
        }

        // Second pass: unconditionally re-check the filesystem.
        //
        // In practice this almost never finds anything, the first pass is
        // already strict. Still, Downloads can theoretically fail in
        // subtle ways (dropped connection, truncated write) so we don't
        // trust the reported state. Instead, we re-run the reconciler
        // against the actual disk to catch stragglers.
        //
        // That is, the real source of truth is the filesystem.
        //
        downloads_.clear ();
        tasks.clear ();

        auto ps (
          cache_.get_reconciler ().plan (m, ct::client, r.client.tag_name));

        size_t n (0);

        for (auto& p: ps)
        {
          if (p.action != reconcile_action::download)
            continue;

          // The reconciler plan might lack the URL so we have to patch it
          // back in from the asset list.
          //
          if (p.url.empty ())
          {
            fs::path d (p.path);
            string fn (d.filename ().string ());

            if (auto i (c_assets.find (fn)); i != c_assets.end ())
              p.url = i->second->browser_download_url;
          }

          if (p.url.empty ())
            continue;

          n++;

          fs::path d (p.path);
          if (d.has_parent_path ())
          {
            error_code ec;
            create_directories (d.parent_path (), ec);
          }

          download_request rq;
          rq.urls.push_back (p.url);
          rq.target = d;
          rq.name = d.filename ().string ();
          rq.expected_size = p.expected_size;

          string nm (rq.name);
          auto t (downloads_.queue_download (move (rq)));
          auto e (progress_.add_entry (nm + " (verify)"));

          e->metrics ().total_bytes.store (p.expected_size, memory_order_relaxed);
          tasks[t] = e;

          t->on_progress = [e, this] (const download_progress& dp)
          {
            progress_.update_progress (e, dp.downloaded_bytes, dp.total_bytes);
          };
        }

        if (n > 0)
        {
          asio::co_spawn (ioc_, downloads_.execute_all (), asio::detached);

          while (downloads_.completed_count () + downloads_.failed_count () <
                downloads_.total_count ())
          {
            // Prune completed or failed tasks from the UI list.
            //
            erase_if (tasks, [&] (const auto& kv)
            {
              if (kv.first->completed () || kv.first->failed ())
              {
                progress_.remove_entry (kv.second);
                return true;
              }
              return false;
            });

            timer.expires_after (chrono::milliseconds (25));
            co_await timer.async_wait (asio::use_awaitable);
          }

          if (downloads_.failed_count () > 0)
          {
            co_await progress_.stop ();
            throw runtime_error ("download failed after verification pass");
          }
        }

        co_await progress_.stop ();
      }

      // Post-process downloads (extraction).
      //
      const auto& root (ctx_.install_location);

      // Track direct downloads first.
      //
      for (const auto& item : plan)
      {
        if (item.action == reconcile_action::download && fs::exists (item.path))
          cache_.track (item.path, item.component, item.version, item.expected_hash);
      }

      unordered_map<string, const ma*> amap;
      for (const auto& a : m.archives)
        amap[manifest_coordinator::resolve_path (a, root).string ()] = &a;

      // Handle archives. If a downloaded item is a zip and appears in our
      // manifest archive list, we extract it and track the contents.
      //
      for (const auto& item : plan)
      {
        if (item.action != reconcile_action::download) continue;

        fs::path p (item.path);
        if (p.extension () != ".zip" && p.extension () != ".ZIP") continue;

        auto it (amap.find (p.string ()));
        if (it == amap.end ()) continue;

        const auto* arch (it->second);

        try
        {
          co_await manifest_coordinator::extract_archive (*arch, p, root);

          vector<fs::path> extracted;
          extracted.reserve (arch->files.size ());

          for (const auto& f : arch->files)
          {
            fs::path ep (manifest_coordinator::resolve_path (f, root));
            extracted.push_back (std::move (ep));
          }

          cache_.track (extracted, item.component, item.version);

          std::error_code ec;
          fs::remove (p, ec);
        }
        catch (const exception& e)
        {
          throw runtime_error ("extraction failure: " + arch->name + ": " + e.what ());
        }
      }

      stamp ();
    }

    asio::awaitable<int>
    execute_payload ()
    {
#ifdef __linux__
      co_return co_await execute_proton ();
#else
      co_return co_await execute_native ();
#endif
    }

#ifdef __linux__
    asio::awaitable<int>
    execute_proton ()
    {
      if (ctx_.proton_binary.empty ())
      {
        cerr << "error: game binary unspecified\n";
        co_return 1;
      }

      fs::path binary_path (ctx_.install_location / ctx_.proton_binary);
      if (!fs::exists (binary_path))
      {
        cerr << "error: game binary not found: " << binary_path << "\n";
        co_return 1;
      }

      proton_coordinator proton (ioc_);

      if (!fs::exists (ctx_.install_location / "steam.exe"))
      {
        cerr << "error: runtime dependency missing: steam.exe\n";
        co_return 1;
      }

      // Try to detect Steam path from environment.
      //
      fs::path steam_root;
      if (const char* home = getenv ("HOME"))
        steam_root = fs::path (home) / ".steam" / "steam";

      bool success (co_await proton.complete_launch (steam_root,
                                                     binary_path,
                                                     10190,
                                                     ctx_.proton_arguments));

      if (!success)
      {
        cerr << "error: execution failed\n";
        co_return 1;
      }

      co_return 0;
    }
#endif

#ifndef __linux__
    // Unlike the Linux/Proton path, there is no strict ABI or namespace
    // boundary that we must bridge prior to launch.
    //
    // While we could technically introduce a pre-launch Steam check here to
    // unify behavior across platforms, it is not structurally required on
    // Windows. The game's own startup logic is capable of handling the "Steam
    // not found" scenario (or lazy-loading the interface) without our help.
    //
    // So for now, we favor correctness-by-minimalism: we treat the launcher as
    // a thin process-spawning layer that mirrors native OS expectations,
    // leaving the heavy lifting to the game itself.
    //
    asio::awaitable<int>
    execute_native ()
    {
      if (ctx_.proton_binary.empty ())
      {
        cerr << "error: game binary unspecified\n";
        co_return 1;
      }

      fs::path binary_path (ctx_.install_location / ctx_.proton_binary);
      if (!fs::exists (binary_path))
      {
        cerr << "error: game binary not found: " << binary_path << "\n";
        co_return 1;
      }

      try
      {
        namespace bp = boost::process;
        bp::child game (
          binary_path.string (),
          bp::args (ctx_.proton_arguments),
          bp::start_dir (ctx_.install_location.make_preferred ().string ())
        );

        game.detach ();
      }
      catch (const exception& e)
      {
        cerr << "error: failed to launch game: " << e.what () << "\n";
        co_return 1;
      }

      co_return 0;
    }
#endif

  private:
    // Report the rate limit backoff status to the user.
    //
    void
    handle_rate_limit_progress (const string& msg, uint64_t rem)
    {
      // The progress UI might not have been started yet (e.g., rate limit hit
      // during initial metadata fetch, before any downloads). Ensure the
      // render loop is running so the dialog is actually visible.
      //
      if (!progress_.running ())
      {
        progress_.start ();
        rate_limit_started_progress_ = true;
      }

      string s (
        msg + "\n\nTime remaining: " + std::to_string (rem) + " seconds");

      progress_.show_dialog ("Rate Limit", s);

      // If the countdown has finished, we can dismiss the dialog and carry on
      // unless we started the progress UI just for this dialog, in which case
      // we must tear it down so it doesn't interfere with the normal flow.
      //
      if (rem == 0)
      {
        progress_.hide_dialog ();

        if (rate_limit_started_progress_)
        {
          asio::co_spawn (ioc_, progress_.stop (), asio::detached);
          rate_limit_started_progress_ = false;
        }
      }
    }

    asio::io_context& ioc_;
    runtime_context ctx_;
    github_coordinator github_;
    http_coordinator http_;
    download_coordinator downloads_;
    progress_coordinator progress_;
    cache_coordinator cache_;
    bool rate_limit_started_progress_ {false};
  };

  // Resolve MW2 installation root via Steam.
  //
  asio::awaitable<optional<fs::path>>
  resolve_root (asio::io_context& ctx)
  {
    try
    {
      auto p (co_await get_mw2_default_path (ctx));

      if (p && fs::exists (*p))
        co_return p;
    }
    catch (const exception&)
    {
      // If something blows up, just fall through to the default.
      //
    }

    // No Steam installation found; use the current directory.
    //
    co_return fs::current_path ();
  }

  // Check for and optionally install launcher updates.
  //
  // Returns true if the launcher is restarting (caller should exit), false
  // otherwise.
  //
  // If `pc` is provided and an update is available, the progress coordinator
  // will be started to display download progress. The UI is only shown when
  // there is actually an update to install, that is, we want to avoid
  // unnecessary UI for up-to-date scenarios.
  //
  asio::awaitable<bool>
  check_self_update (asio::io_context& io,
                     bool pre,
                     bool only,
                     progress_coordinator* pc = nullptr)
  {
    auto uc (make_update_coordinator (io));
    uc->set_include_prerelease (pre);

    // If the user explicitly requested an update, we let the coordinator
    // attempt the restart immediately after installation.
    //
    uc->set_auto_restart (only);

    // Wire up rate limit progress callback on the discovery layer's GitHub API
    // so the user sees a countdown if GitHub throttles.
    //
    bool rl_started_ui (false);

    uc->discovery ().set_progress_callback (
      [pc, &io, &rl_started_ui] (const string& message, uint64_t rem)
    {
      if (pc == nullptr)
        return;

      if (!pc->running ())
      {
        pc->start ();
        rl_started_ui = true;
      }

      string s (message + "\n\nTime remaining: " + std::to_string (rem) +
                " seconds");

      pc->show_dialog ("Rate Limit", s);

      if (rem == 0)
      {
        pc->hide_dialog ();

        if (rl_started_ui)
        {
          asio::co_spawn (io, pc->stop (), asio::detached);
          rl_started_ui = false;
        }
      }
    });

    try
    {
      // First, check if an update is available without starting the UI.
      //
      auto s (co_await uc->check_for_updates ());

      // If we were strictly asked to update but couldn't even check (e.g.,
      // network down), that's fatal. Otherwise, we treat it as a soft failure
      // and proceed to the application.
      //
      if (s == update_status::check_failed && only)
      {
        cerr << "error: failed to check for launcher updates" << endl;
        co_return false;
      }

      // No update available, nothing to do.
      //
      if (s == update_status::up_to_date)
        co_return false;

      // An update is available. Now we can start the UI if requested.
      //
      if (pc != nullptr)
      {
        uc->set_progress_coordinator (pc);
        pc->start ();
      }

      // Proceed to install the update.
      //
      const auto& info (uc->last_update_info ());

      cout << "launcher update available: " << info.version
           << " (current: " << uc->current_version () << ")" << endl;

      if (!info.body.empty ())
        cout << "Release notes:\n" << info.body << "\n" << endl;

      auto r (co_await uc->install_update (info));

      if (!r.success)
      {
        cerr << "error: update failed: " << r.error_message << endl;
        co_return false;
      }

      // Try to restart into the new version.
      //
      if (uc->restart ())
        co_return true;

      cerr << "warning: failed to restart launcher automatically" << endl;
      cout << "please restart the launcher manually." << endl;

      co_return false;
    }
    catch (const exception& e)
    {
      // Same logic as above: complain loudly if this was the only task,
      // otherwise just warn and move on.
      //
      if (only)
        cerr << "error: update failed: " << e.what () << endl;
      else
        cerr << "warning: update check failed: " << e.what () << endl;

      co_return false;
    }
  }
}

int
main (int argc, char* argv[])
{
  using namespace std;
  using namespace launcher;

  try
  {
    // Change the current working directory to the location of the launcher
    // executable. This is important because the launcher is often invoked from
    // a different directory (e.g., via command line), but it needs to operate
    // relative to its own location, not the shell's working directory.
    //
    {
      error_code ec;
      fs::path exe;

#ifdef _WIN32
      // On Windows, use GetModuleFileName.
      //
      wchar_t buf[MAX_PATH];
      if (GetModuleFileNameW (nullptr, buf, MAX_PATH) != 0)
        exe = fs::path (buf);
#else
      // On Linux/Unix, /proc/self/exe is a symlink to the executable.
      //
      exe = fs::canonical ("/proc/self/exe", ec);
#endif

      if (!exe.empty () && !ec)
      {
        fs::path dir (exe.parent_path ());
        fs::current_path (dir, ec);

        if (ec)
          cerr << "warning: unable to change to launcher directory: "
               << ec.message () << endl;
      }
    }

    options opt (argc, argv);

    // Handle --version.
    //
    if (opt.version ())
    {
      auto& o (cout);

      o << "Launcher " << HELLO_VERSION_ID << "\n";

      return 0;
    }

    // Handle --help.
    //
    if (opt.help ())
    {
      auto& o (cout);

      o << "usage: launcher [options]"   << "\n"
        << "options:"                 << "\n";

      opt.print_usage (o);

      return 0;
    }

    asio::io_context ioc;
    runtime_context ctx;

    // Determine the installation path.
    //
    if (!opt.path_specified ())
    {
      optional<fs::path> heuristic;

      asio::co_spawn (
        ioc,
        resolve_root (ioc),
        [&heuristic, &ioc] (exception_ptr ex, optional<fs::path> p)
        {
          if (!ex) heuristic = p;
          ioc.stop ();
        });

      // Run the detection loop. We restart the context afterwards because
      // stop() makes it return, and we need it fresh for the launcher itself.
      //
      ioc.restart ();
      ioc.run ();
      ioc.restart ();

      if (!heuristic)
      {
        // User cancelled or no Steam installation found.
        //
        return 1;
      }

      ctx.install_location = *heuristic;
    }
    else
      ctx.install_location = fs::path (opt.path ());

    // Map the command line options to our context.
    //
    ctx.upstream_owner = "iw4x";
    ctx.upstream_repo = "iw4x-client";
    ctx.prerelease = opt.prerelease ();
    ctx.concurrency_limit = opt.jobs ();

    // Execution settings.
    //
    ctx.proton_binary = opt.game_exe ();

    if (opt.game_args_specified ())
      ctx.proton_arguments = opt.game_args ();

    if (!opt.no_self_update () || opt.self_update_only ())
    {
      bool r (false);

      // Setup the progress feedback. The progress coordinator is passed to
      // check_self_update but only started if an update is actually available.
      //
      auto pc (make_unique<progress_coordinator> (ioc));

      asio::co_spawn (
        ioc,
        check_self_update (ioc,
                           opt.prerelease (),
                           opt.self_update_only (),
                           pc.get ()),
        [&r, &ioc, &pc] (exception_ptr e, bool v)
        {
          if (!e)
            r = v;

          if (pc != nullptr)
            asio::co_spawn (ioc, pc->stop (), asio::detached);

          ioc.stop ();
        });

      ioc.restart ();
      ioc.run ();
      ioc.restart ();

      // If we applied an update that requires a restart (e.g. replaced the
      // executable) or if the user only asked to update, we are done.
      //
      if (r || opt.self_update_only ())
        return 0;
    }

    // Control Loop.
    //
    int exit_code (0);
    launcher_controller controller (ioc, move (ctx));

    asio::co_spawn (
      ioc,
      controller.run (),
      [&exit_code, &ioc] (exception_ptr ex, int r)
      {
        exit_code = r;
        if (ex)
        {
          try { rethrow_exception (ex); }
          catch (const exception& e)
          {
            cerr << "error: " << e.what () << "\n";
            exit_code = 1;
          }
        }
        ioc.stop ();
      });

    ioc.run ();
    return exit_code;
  }
  catch (const cli::exception& ex)
  {
    cerr << "error: " << ex.what () << "\n";
    return 1;
  }
  catch (const exception& ex)
  {
    cerr << "error: " << ex.what () << "\n";
    return 1;
  }
}
