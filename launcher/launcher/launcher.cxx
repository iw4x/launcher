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
#include <boost/process.hpp>

#include <launcher/launcher-download.hxx>
#include <launcher/launcher-github.hxx>
#include <launcher/launcher-http.hxx>
#include <launcher/launcher-manifest.hxx>
#include <launcher/launcher-options.hxx>
#include <launcher/launcher-progress.hxx>
#include <launcher/launcher-steam.hxx>
#ifdef __linux__
#  include <launcher/launcher-steam-proton.hxx>
#endif
#include <launcher/version.hxx>

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
  // Prompt the user for a Yes/No answer.
  //
  // We strictly require a 'y' or 'n' (case-insensitive) to proceed. While
  // defaulting on EOF might seem convenient, it's safer to bail out if the
  // input stream is broken or closed unexpectedly.
  //
  static bool
  confirm_action (const string& prompt, char def = '\0')
  {
    string a;
    do
    {
      cout << prompt << ' ';

      // Note: getline() sets the failbit if it fails to extract anything, and
      // the eofbit if it hits EOF before the delimiter.
      //
      getline (cin, a);

      bool f (cin.fail ());
      bool e (cin.eof ());

      // If we hit EOF or fail without a newline, force one out so the next
      // output doesn't get messed up.
      //
      if (f || e)
        cout << endl;

      if (f)
        throw ios_base::failure ("unable to read y/n answer from stdin");

      if (a.empty () && def != '\0')
      {
        // We don't want to treat EOF as the default answer; we want to see an
        // actual newline from the user to confirm they are present and paying
        // attention.
        //
        if (!e)
          a = def;
      }
    } while (a != "y" && a != "Y" && a != "n" && a != "N");

    return a == "y" || a == "Y";
  }

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
      d = fs::path (v) / "iw4x-launcher";
    else if (const char* v = getenv ("APPDATA"))
      d = fs::path (v) / "iw4x-launcher";
    else
      d = fs::current_path () / ".launcher-cache";
#elif defined(__APPLE__)
    if (const char* h = getenv ("HOME"))
      d = fs::path (h) / "Library" / "Application Support" / "iw4x-launcher";
    else
      d = fs::current_path () / ".launcher-cache";
#else
    // On Linux/Unix, we respect the XDG Base Directory specification.
    //
    if (const char* v = getenv ("XDG_CACHE_HOME"))
      d = fs::path (v) / "iw4x";
    else if (const char* h = getenv ("HOME"))
      d = fs::path (h) / ".cache" / "iw4x";
    else
      d = fs::current_path () / ".launcher-cache";
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
      d = fs::current_path () / ".launcher-cache";

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
    bool           force_verification;
    bool           disable_integrity_check;
    size_t         concurrency_limit;
    bool           headless;
    bool           enable_execution;
    bool           use_proton;
    fs::path       proton_binary;
    fs::path       proton_steam_root;
    fs::path       proton_helper_override;
    uint32_t       proton_appid;
    vector<string> proton_arguments;
    bool           verbose_proton;
    bool           proton_logging;
  };

  // Manages version pinning markers and the archive extraction cache.
  //
  class persistence_layer
  {
  public:
    struct version_snapshot
    {
      string client;
      string raw;
      string helper;
    };

    explicit
    persistence_layer (const fs::path& install_root)
      : root_ (resolve_cache_root (install_root)),
        marker_installed_ (root_ / ".launcher-installed"),
        marker_ver_client_ (root_ / ".launcher-version-client"),
        marker_ver_raw_ (root_ / ".launcher-version-raw"),
        marker_ver_helper_ (root_ / ".launcher-version-helper"),
        archive_cache_path_ (root_ / ".launcher-archive.json")
    {
    }

    const fs::path&
    archive_cache_path () const
    {
      return archive_cache_path_;
    }

    // Check if the installation is complete and valid.
    //
    bool
    is_fully_installed () const
    {
      return fs::exists (marker_installed_) &&
             fs::exists (marker_ver_client_) &&
             fs::exists (marker_ver_raw_) &&
             fs::exists (marker_ver_helper_);
    }

    version_snapshot
    read_versions () const
    {
      return {read_file (marker_ver_client_),
              read_file (marker_ver_raw_),
              read_file (marker_ver_helper_)};
    }

    // Atomically-ish commit the new versions.
    //
    void
    commit_versions (const version_snapshot& v)
    {
      write_file (marker_ver_client_, v.client);
      write_file (marker_ver_raw_, v.raw);
      write_file (marker_ver_helper_, v.helper);

      ofstream os (marker_installed_);
    }

  private:
    static string
    read_file (const fs::path& p)
    {
      ifstream is (p);
      string s;
      getline (is, s);
      return s;
    }

    static void
    write_file (const fs::path& p, const string& content)
    {
      ofstream os (p);
      os << content;
    }

    fs::path root_;
    fs::path marker_installed_;
    fs::path marker_ver_client_;
    fs::path marker_ver_raw_;
    fs::path marker_ver_helper_;
    fs::path archive_cache_path_;
  };

  // Aggregates remote state required for synchronization.
  //
  struct remote_state
  {
    github_release client;
    github_release raw;
    github_release helper;
    string dlc_manifest_json;

    persistence_layer::version_snapshot
    to_snapshot () const
    {
      return {client.tag_name, raw.tag_name, helper.tag_name};
    }
  };

  // Main controller for the bootstrap process.
  //
  class launcher_controller
  {
  public:
    launcher_controller (asio::io_context& ioc, runtime_context ctx)
        : ioc_ (ioc),
          ctx_ (move (ctx)),
          state_ (ctx_.install_location),
          github_ (ioc_),
          http_ (ioc_),
          downloads_ (ioc_, ctx_.concurrency_limit),
          progress_ (ioc_)
    {
      github_.set_progress_callback (
        [this] (const string& message, uint64_t seconds_remaining)
      {
        this->handle_rate_limit_progress (message, seconds_remaining);
      });
    }

    asio::awaitable<int>
    run ()
    {
      if (!ctx_.headless)
        progress_.start ();

      // Discovery Phase.
      //
      // We resolve the remote state of all components (Client, Rawfiles,
      // Helper, and DLC) in parallel to minimize latency. We don't log here yet
      // because we want the first output to be the result ("Up to date" or
      // "Update available").
      //
      remote_state remote (co_await resolve_remote_state ());

      // Verification Phase.
      //
      // If our local version markers match the remote tags, we assume the core
      // components are up to date and can skip the expensive manifest
      // resolution.
      //
      bool up_to_date (state_.is_fully_installed () &&
                       compare_versions (state_.read_versions (),
                                         remote.to_snapshot ()));

      if (up_to_date && !ctx_.force_verification)
      {
        if (ctx_.headless)
          cout << "Client is up to date (" << remote.client.tag_name << ").\n";
        else
          co_await progress_.stop ();

        co_return co_await execute_payload ();
      }

      // Provisioning Phase.
      //
      // State is divergent or verification was forced. We need to calculate the
      // diff and synchronize artifacts.
      //
      co_await reconcile_artifacts (remote);

      // Commit Phase.
      //
      // If we made it this far, update the version markers to reflect the new
      // state.
      //
      state_.commit_versions (remote.to_snapshot ());

      if (ctx_.headless)
        cout << "Update complete.\n";
      else
        co_await progress_.stop ();

      // Execution Phase.
      //
      co_return co_await execute_payload ();
    }

  private:
    void
    log (const string& msg)
    {
      if (ctx_.headless)
        cout << msg << "\n";
      else
        progress_.add_log (msg);
    }

    bool
    compare_versions (const persistence_layer::version_snapshot& local,
                      const persistence_layer::version_snapshot& remote)
    {
      return local.client == remote.client &&
             local.raw    == remote.raw &&
             local.helper == remote.helper;
    }

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

    // Synchronize local filesystem with the remote manifest.
    //
    asio::awaitable<void>
    reconcile_artifacts (const remote_state& remote)
    {
      // Load extraction cache so we don't re-extract archives we've already
      // processed.
      //
      archive_cache ac (state_.archive_cache_path ());
      try
      {
        ac.load ();
      }
      catch (const exception& e)
      {
        if (ctx_.headless)
          cerr << "warning: archive cache corruption detected: "
               << e.what () << "\n";
      }

      log ("Downloading manifest...");

      // We only need to fetch the client manifest (the DLC manifest was
      // prefetched during discovery).
      //
      manifest m (co_await github_.fetch_manifest (remote.client));

      // Inject assets from secondary repositories.
      //
      // Rawfiles and the steam helper come from different repositories, but we
      // treat them as "archives" in the manifest system so they can be
      // downloaded and verified using the same pipeline.
      //
      auto inject_assets = [&m] (const vector<github_asset>& assets,
                                 const string& filter = "")
      {
        int count (0);
        for (const auto& a : assets)
        {
          if (!filter.empty () && a.name != filter)
            continue;

          manifest_archive ma;
          ma.name = a.name;
          ma.url = a.browser_download_url;
          ma.size = a.size;
          m.archives.push_back (move (ma));
          count++;
        }
        return count;
      };

#ifdef __linux__
      inject_assets (remote.helper.assets, "steam.exe");
      inject_assets (remote.helper.assets, "steam_api64.dll ");
      log ("Added steam helper");
#endif

      int raw_count (inject_assets (remote.raw.assets));
      log ("Added " + std::to_string (raw_count) + " rawfiles");

      // Merge dlc.
      //
      // We can't reuse inject_assets() here because the data source differs:
      // DLCs are parsed from a custom JSON manifest (manifest_file objects)
      // rather than GitHub API assets. The URL logic is also CDN-specific.
      //
      if (!remote.dlc_manifest_json.empty ())
      {
        manifest dlc (remote.dlc_manifest_json, manifest_format::dlc);

        for (const auto& f : dlc.files)
        {
          if (f.path.empty ())
            continue;

          manifest_archive ma;
          ma.name = f.path;
          ma.url = "https://cdn.iw4x.io/" + f.path;
          ma.size = f.size;
          ma.hash = f.hash;
          m.archives.push_back (move (ma));
        }

        log ("Added " + std::to_string (dlc.files.size ()) + " DLC");
      }

      // With the full manifest built, we can now diff it against the local
      // filesystem to see what's missing or outdated.
      //
      log ("Checking local files...");

      vector<manifest_file> missing_files (
        manifest_coordinator::get_missing_files (m, ctx_.install_location, false));

      vector<manifest_archive> missing_archives (
        manifest_coordinator::get_missing_archives (m, ctx_.install_location, &ac));

      if (missing_files.empty () && missing_archives.empty ())
        co_return;

      // Queue acquisition tasks.
      //
      uint64_t total_bytes (0);
      for (const auto& f : missing_files) total_bytes += f.size;
      for (const auto& a : missing_archives) total_bytes += a.size;

      if (ctx_.headless)
        cout << "Need to download "
             << (missing_files.size () + missing_archives.size ()) << " items ("
             << format_bytes (total_bytes) << ")\n";

      unordered_map<shared_ptr<download_coordinator::task_type>,
                    shared_ptr<progress_entry>> task_map;

      auto schedule_download = [&] (const string& url,
                                    const fs::path& target,
                                    const string& name,
                                    uint64_t size,
                                    const auto& hash)
      {
        download_request req;
        req.urls.push_back (url);
        req.target = target;
        req.name = name;
        req.expected_size = size;

        if (!hash.empty ())
        {
          req.verification_method = ctx_.disable_integrity_check
            ? download_verification::none
            : download_verification::sha256;
          req.verification_value = hash.string ();
        }
        else
        {
          req.verification_method = download_verification::none;
        }

        auto task (downloads_.queue_download (move (req)));

        if (!ctx_.headless)
        {
          auto entry (progress_.add_entry (name));
          task_map[task] = entry;
          entry->metrics ().total_bytes.store (size, memory_order_relaxed);

          task->on_progress = [entry, this] (const download_progress& p)
          {
            progress_.update_progress (entry,
                                       p.downloaded_bytes,
                                       p.total_bytes);
          };
        }
      };

      for (const auto& f : missing_files)
      {
        if (!f.asset_name)
          continue;

        auto a (github_.find_asset (remote.client, *f.asset_name));

        if (!a)
        {
          cerr << "warning: asset not found for file: " << f.path << "\n";
          continue;
        }

        fs::path t (
          manifest_coordinator::resolve_path (f, ctx_.install_location));

        if (t.has_parent_path ())
          fs::create_directories (t.parent_path ());

        schedule_download (a->browser_download_url,
                           t,
                           t.filename ().string (),
                           f.size,
                           f.hash);
      }

      for (const auto& a : missing_archives)
      {
        if (a.url.empty ()) continue;

        fs::path t (
          manifest_coordinator::resolve_path (a, ctx_.install_location));

        if (t.has_parent_path ())
          fs::create_directories (t.parent_path ());

        schedule_download (a.url, t, a.name, a.size, a.hash);
      }

      asio::co_spawn (ioc_, downloads_.execute_all (), asio::detached);

      // Drain the queue.
      //
      while (downloads_.completed_count () + downloads_.failed_count () <
             downloads_.total_count ())
      {
        if (!ctx_.headless)
        {
          for (auto it (task_map.begin ()); it != task_map.end (); )
          {
            if (it->first->completed () || it->first->failed ())
            {
              progress_.remove_entry (it->second);
              it = task_map.erase (it);
            }
            else ++it;
          }
        }
        asio::steady_timer timer (ioc_, chrono::milliseconds (100));
        co_await timer.async_wait (asio::use_awaitable);
      }

      if (downloads_.failed_count () > 0)
        throw runtime_error ("download failed");

      // Materialize artifacts.
      //
      // Only .zip files need extraction; .iwd, .ff, and .exe files are
      // already in their final form.
      //
      for (const auto& a : missing_archives)
      {
        fs::path p (manifest_coordinator::resolve_path (a, ctx_.install_location));
        string ext (p.extension ().string ());
        transform (ext.begin (), ext.end (), ext.begin (),
                   [] (unsigned char c) { return tolower (c); });

        if (ext == ".zip" && fs::exists (p))
        {
          try
          {
            co_await manifest_coordinator::extract_archive (
              a,
              p,
              ctx_.install_location,
              &ac);

            fs::remove (p);
          }
          catch (const exception& e)
          {
            throw runtime_error ("extraction failure: " + a.name + ": " + e.what ());
          }
        }
      }

      try { ac.save (); } catch (...) {}
    }

    asio::awaitable<int>
    execute_payload ()
    {
      if (!ctx_.enable_execution)
        co_return 0;

#ifdef __linux__
      if (ctx_.use_proton)
        co_return co_await execute_proton ();
#else
      co_return co_await execute_native ();
#endif

      if (ctx_.headless)
        cout << "Native execution requested (not implemented).\n";

      co_return 0;
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

      log ("Starting Proton...");

      proton_coordinator proton (ioc_);
      proton.set_verbose (ctx_.verbose_proton);
      proton.set_enable_logging (ctx_.proton_logging);

      // If we have a custom helper (steam.exe), copy it in.
      //
      // Mostly useful for development/debugging where we might want to test a
      // specific steam helper build without pushing it to GitHub.
      //
      if (!ctx_.proton_helper_override.empty ())
      {
        if (fs::exists (ctx_.proton_helper_override))
        {
          try
          {
            fs::copy_file (ctx_.proton_helper_override,
                           ctx_.install_location / "steam.exe",
                           fs::copy_options::overwrite_existing);
          }
          catch (const exception& e)
          {
            cerr << "warning: failed to inject helper override: "
                 << e.what () << "\n";
          }
        }
        else
        {
          cerr << "warning: helper override not found: "
               << ctx_.proton_helper_override << "\n";
        }
      }

      if (!fs::exists (ctx_.install_location / "steam.exe"))
      {
        cerr << "error: runtime dependency missing: steam.exe\n";
        co_return 1;
      }

      bool success (co_await proton.complete_launch (ctx_.proton_steam_root,
                                                     binary_path,
                                                     ctx_.proton_appid,
                                                     ctx_.proton_arguments));

      if (!success)
      {
        cerr << "error: execution failed\n";
        co_return 1;
      }

      if (ctx_.headless)
        cout << "Game launched.\n";

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

      log ("Starting game...");

      try
      {
        namespace bp = boost::process;
        bp::child game (
          binary_path.string (),
          bp::args (ctx_.proton_arguments),
          bp::start_dir (ctx_.install_location)
        );

        game.detach ();
      }
      catch (const exception& e)
      {
        cerr << "error: failed to launch game: " << e.what () << "\n";
        co_return 1;
      }

      if (ctx_.headless)
        cout << "Game launched.\n";

      co_return 0;
    }
#endif

  private:
    // Report the rate limit backoff status to the user.
    //
    void
    handle_rate_limit_progress (const string& msg, uint64_t rem)
    {
      // If we are running with the UI, pop up a dialog with the countdown so
      // the user knows exactly why we are stalled.
      //
      if (!ctx_.headless)
      {
        string s (
          msg + "\n\nTime remaining: " + std::to_string (rem) + " seconds");

        progress_.show_dialog ("Rate Limit", s);

        // If the countdown has finished, we can dismiss the dialog and
        // carry on.
        //
        if (rem == 0)
          progress_.hide_dialog ();
      }
      // Otherwise, if we are headless, just dump it to the console.
      //
      else
        cout << msg << " (" << rem << " seconds remaining)\n";
    }

    asio::io_context& ioc_;
    runtime_context ctx_;
    persistence_layer state_;
    github_coordinator github_;
    http_coordinator http_;
    download_coordinator downloads_;
    progress_coordinator progress_;
  };

  // Resolve MW2 installation root via Steam.
  //
  // If we find something, we check our local cache to see if the user has
  // previously made a decision about this path to avoid pestering them on every
  // run.
  //
  asio::awaitable<optional<fs::path>>
  resolve_install_root (asio::io_context& ioc)
  {
    try
    {
      auto p (co_await get_mw2_default_path (ioc));

      if (p && fs::exists (*p))
      {
        fs::path cache (resolve_cache_root ());
        string digest (path_digest (*p));

        fs::path marker_y (cache / (digest + ".yes"));
        fs::path marker_n (cache / (digest + ".no"));

        bool has_y (fs::exists (marker_y));
        bool has_n (fs::exists (marker_n));

        // If we have both markers, the cache is inconsistent (maybe user
        // fiddling or a race condition). Better to wipe both and ask again.
        //
        if (has_y && has_n)
        {
          fs::remove (marker_y);
          fs::remove (marker_n);
          has_y = has_n = false;
        }

        if (has_y || has_n)
        {
          // Using cached preference.
          //
          if (has_y)
            co_return p;
        }
        else
        {
          cout
            << "Found Steam installation of Call of Duty: Modern Warfare 2:\n"
            << "  " << p->string () << "\n\n";

          bool accepted (
            confirm_action (
              "Install IW4x to this directory? [Y/n] "
              "(n = use current directory)", 'y'));

          // Cache the answer by touching a marker file.
          //
          { ofstream os (accepted ? marker_y : marker_n); }

          if (accepted)
            co_return p;
        }
      }
    }
    catch (const exception&)
    {
      // Silently fall back to the current directory if detection fails.
      //
    }

    co_return nullopt;
  }
}

int
main (int argc, char* argv[])
{
  using namespace std;
  using namespace launcher;

  try
  {
    options opt (argc, argv);

    // Handle --build2-metadata (see also buildfile).
    //
    if (opt.build2_metadata_specified ())
    {
      auto& o (cout);

      // The export.metadata variable must be the first non-blank, non-comment
      // line.
      //
      o << "# build2 buildfile launcher"                                  << "\n"
        << "export.metadata = 1 launcher"                                 << "\n"
        << "launcher.name = [string] launcher"                            << "\n"
        << "launcher.version = [string] '"  << HELLO_VERSION_FULL << '\'' << "\n"
        << "launcher.checksum = [string] '" << HELLO_VERSION_FULL << '\'' << "\n";

      return 0;
    }

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
        resolve_install_root (ioc),
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

      ctx.install_location = heuristic.value_or (fs::current_path ());
    }
    else
      ctx.install_location = fs::path (opt.path ());

    // Platform Configuration.
    //
#ifdef __linux__
    ctx.use_proton = true;
#else
    ctx.use_proton = false;
#endif

    // Map the command line options to our context.
    //
    ctx.upstream_owner = "iw4x";
    ctx.upstream_repo = "iw4x-client";
    ctx.prerelease = opt.prerelease ();
    ctx.force_verification = opt.force_update ();
    ctx.headless = opt.no_ui ();
    ctx.disable_integrity_check = opt.disable_checksum ();
    ctx.concurrency_limit = opt.jobs ();

    // Execution settings.
    //
    ctx.enable_execution = opt.launch ();
    ctx.proton_binary = opt.game_exe ();
    ctx.proton_appid = opt.proton_app_id ();
    ctx.verbose_proton = opt.proton_verbose ();
    ctx.proton_logging = opt.proton_log ();

    if (opt.game_args_specified ())
      ctx.proton_arguments = opt.game_args ();

    if (opt.steam_path_specified ())
      ctx.proton_steam_root = opt.steam_path ();
    else
      // Try to detect Steam path from environment.
      //
      if (const char* home = getenv ("HOME"))
        ctx.proton_steam_root = fs::path (home) / ".steam" / "steam";

    if (opt.steam_helper_specified ())
      ctx.proton_helper_override = opt.steam_helper ();

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
