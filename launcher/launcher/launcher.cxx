#ifdef _WIN32
#  include <winsock2.h>
#  include <windows.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/process.hpp>

#include <launcher/launcher-cache.hxx>
#include <launcher/launcher-download.hxx>
#include <launcher/launcher-github.hxx>
#include <launcher/launcher-http.hxx>
#include <launcher/launcher-log.hxx>
#include <launcher/launcher-manifest.hxx>
#include <launcher/launcher-options.hxx>
#include <launcher/launcher-progress.hxx>
#include <launcher/launcher-steam.hxx>
#include <launcher/launcher-update.hxx>

#ifdef __linux__
#  include <launcher/launcher-steam-proton.hxx>
#endif

#include <launcher/version.hxx>

namespace process = boost::process;
namespace asio = boost::asio;
namespace views = std::views;
namespace ranges = std::ranges;

using namespace std;
using namespace std::filesystem;
using namespace boost::asio::experimental::awaitable_operators;

namespace launcher
{
  namespace
  {
    constexpr auto
    info ([] (auto&&... args)
    {
      log::info (categories::launcher (),
                 std::forward<decltype (args)> (args)...);
    });

    constexpr auto
    trace_l3 ([] (auto&&... args)
    {
      log::trace_l3 (categories::launcher (),
                     std::forward<decltype (args)> (args)...);
    });

    constexpr auto
    trace_l2 ([] (auto&&... args)
    {
      log::trace_l2 (categories::launcher (),
                     std::forward<decltype (args)> (args)...);
    });

    constexpr auto
    error ([] (auto&&... args)
    {
      log::error (categories::launcher (),
                  std::forward<decltype (args)> (args)...);
    });

    constexpr auto
    warning ([] (auto&&... args)
    {
      log::warning (categories::launcher (),
                  std::forward<decltype (args)> (args)...);
    });

    string
    to_utf8 (const path& p)
    {
      auto s (p.u8string ());
      return string (s.begin (), s.end ());
    }

    path
    from_utf8 (string_view s)
    {
      return path (reinterpret_cast<const char8_t*> (s.data ()),
                   reinterpret_cast<const char8_t*> (s.data () + s.size ()));
    }

    void
    reanchor_cwd ()
    {
      error_code ec;

#ifdef _WIN32
      vector<wchar_t> b (32768);
      DWORD n (GetModuleFileNameW (nullptr,
                                   b.data (),
                                   static_cast<DWORD> (b.size ())));

      if (n == 0)
        throw system_error (GetLastError (),
                            system_category (),
                            "GetModuleFileNameW failed");

      fs::path p (wstring_view (b.data (), n));
#else
      path p (canonical ("/proc/self/exe", ec));

      if (ec)
        throw system_error (ec, "failed to resolve /proc/self/exe");
#endif

      if (p.empty ())
        throw runtime_error ("executable path resolved to an empty string");

      path d (p.parent_path ());
      current_path (d, ec);

      if (ec)
        throw system_error (ec,
                            "failed to change working directory to: " +
                              to_utf8 (d));

      trace_l2 ("changed working directory to: {}", to_utf8 (d));
    }

    template <typename Coordinator>
    void
    bind_rate_limit_ui (asio::io_context& io_ctx,
                        Coordinator& coord,
                        progress_coordinator& progress_coord)
    {
      auto is_ui_active (make_shared<bool> (false));

      auto callback (
        [&io_ctx, &progress_coord, is_ui_active] (const string& message,
                                                  uint64_t remaining_time)
      {
        if (!progress_coord.running ())
        {
          progress_coord.start ();
          *is_ui_active = true;
        }

        string display_str (message + "\n\nTime remaining: " +
                            std::to_string (remaining_time) + " seconds");

        progress_coord.show_dialog ("Rate Limit", display_str);

        if (remaining_time == 0)
        {
          progress_coord.hide_dialog ();

          if (*is_ui_active)
          {
            asio::co_spawn (io_ctx, progress_coord.stop (), asio::detached);
            *is_ui_active = false;
          }
        }
      });

      if constexpr (requires (decltype (callback) func) {
                      coord.discovery ().set_progress_callback (func);
                    })
        coord.discovery ().set_progress_callback (callback);
      else
        coord.set_progress_callback (callback);
    }
  }

  string
  path_digest (const path& p)
  {
    std::hash<string> h;
    string r (std::to_string (h (to_utf8 (p))));

    trace_l3 ("computed path digest for {}: {}", to_utf8 (p), r);
    return r;
  }

  path
  resolve_cache_root (const path& s = {})
  {
    trace_l2 ("resolving cache root {}", to_utf8 (s));
    path d;

#ifdef _WIN32
    // On Windows, the standard getenv() uses the ANSI codepage which corrupts
    // Unicode characters. We must use _wgetenv to retrieve paths containing
    // non-ASCII characters.
    //
    if (const wchar_t* v = _wgetenv (L"LOCALAPPDATA"))
      d = path (v) / "iw4x";
    else if (const wchar_t* v = _wgetenv (L"APPDATA"))
      d = path (v) / "iw4x";
    else
      d = current_path () / ".iw4x";
#elif defined(__APPLE__)
    if (const char* h = getenv ("HOME"))
      d = path (h) / "Library" / "Application Support" / "iw4x";
    else
      d = current_path () / ".iw4x";
#else
    if (const char* v = getenv ("XDG_CACHE_HOME"))
      d = path (v) / "iw4x";
    else if (const char* h = getenv ("HOME"))
      d = path (h) / ".cache" / "iw4x";
    else
      d = current_path () / ".iw4x";
#endif

    // If we are looking for the cache specific to an installation (to avoid
    // conflicts between multiple installs), append its unique key.
    //
    if (!s.empty ())
    {
      d /= path_digest (s);
      trace_l3 ("appended scope digest to cache root: {}", to_utf8 (d));
    }

    error_code ec;

    d = weakly_canonical (d, ec);

    if (ec)
      throw system_error (ec,
                          "failed to canonicalize cache directory: " +
                            to_utf8 (d));

    create_directories (d, ec);

    if (ec)
      throw system_error (ec,
                          "failed to create cache directory: " + to_utf8 (d));

    return d;
  }

  asio::awaitable<void>
  check_self_update (asio::io_context& io,
                      bool p,
                      bool o,
                      progress_coordinator& pc)
  {
    info ("checking for launcher updates (prerelease: {}, update_only: {})",
          p,
          o);

    auto uc (make_update_coordinator (io));
    uc->set_include_prerelease (p);
    uc->set_auto_restart (o);

    bind_rate_limit_ui (io, *uc, pc);

    auto s (co_await uc->check_for_updates ());

    if (s == update_status::check_failed && o)
      throw runtime_error ("only update requested but check failed");

    if (s == update_status::up_to_date)
    {
      info ("launcher is up to date");
      co_return;
    }

    info ("launcher update available, proceeding with installation");

    uc->set_progress_coordinator (&pc);
    pc.start ();

    const auto& i (uc->last_update_info ());

   //info ("launcher update available: {} (current: {})",
   //      i.version,
   //      uc->current_version ());

    auto r (co_await uc->install_update (i));

    if (!r.success)
      throw runtime_error ("update failed to install: " + r.error_message);

    info ("restarting into new launcher version");
    uc->restart ();
  }

  asio::awaitable<void>
  execute_plan (asio::io_context& io,
                download_coordinator& dc,
                progress_coordinator& pc,
                cache_coordinator& cc,
                const vector<reconcile_item>& pl,
                const manifest& md,
                const path& ir)
  {
    struct dl_info
    {
      string         url;
      path           tmp;
      path           dst;
      uint64_t       size;
      component_type comp;
      string         ver;
      string         hash;
      int            retries;
      bool           done;
    };

    error_code e;
    path sd (weakly_canonical (resolve_cache_root (ir) / "staging", e));

    if (e)
      throw system_error (e,
                          "failed to canonicalize staging directory: " +
                            to_utf8 (sd));

    create_directories (sd, e);

    if (e)
      throw system_error (e,
                          "failed to create staging directory: " +
                            to_utf8 (sd));

    // Extract only items that require downloading and build tracking
    // structures.
    //
    // Note that we use hashes of the destination paths in the staging area to
    // avoid name collisions if two files have the same basename but belong to
    // different subdirectories.
    //
    auto is_dl ([] (const auto& i)
    {
      return i.action == reconcile_action::download;
    });

    auto to_dl ([&sd] (const auto& i)
    {
      if (i.url.empty ())
        throw runtime_error (
          "reconciliation failed: missing download URL for " + i.path);

      path d (from_utf8 (i.path));
      path t (sd / (path_digest (d) + "_" + to_utf8 (d.filename ())));

      return dl_info {i.url,
                      t,
                      d,
                      i.expected_size,
                      i.component,
                      i.version,
                      i.expected_hash,
                      0,
                      false};
    });

    auto ds (pl | views::filter (is_dl) | views::transform (to_dl) |
             ranges::to<vector> ());

    if (ds.empty ())
      co_return;

    struct active_task
    {
      shared_ptr<download_coordinator::task_type> h;
      shared_ptr<progress_entry> ui;
      dl_info* ctx;

      active_task (shared_ptr<download_coordinator::task_type> h_,
                   shared_ptr<progress_entry> ui_,
                   dl_info* ctx_)
        : h (std::move (h_)),
          ui (std::move (ui_)),
          ctx (ctx_)
      {
      }
    };

    // Loop as long as there are incomplete downloads that have not exhausted
    // their retry budget.
    //
    // Notice that network drops are common during bulk asset fetching. We are
    // slightly forgiving here and allow a few retries before failing.
    //
    auto is_pd ([] (const auto& d)
    {
      return !d.done && d.retries < 3;
    });

    bool hp (ranges::any_of (ds, is_pd));

    while (hp)
    {
      vector<active_task> ts;

      auto pds (ds | views::filter (is_pd));

      for (auto& d : pds)
      {
        download_request r;
        r.urls.push_back (d.url);
        r.target = d.tmp;
        r.name = to_utf8 (d.dst.filename ());
        r.expected_size = d.size;

        auto en (pc.add_entry (r.name));
        auto t (dc.queue_download (std::move (r)));

        en->metrics ().total_bytes.store (d.size, memory_order_relaxed);

        t->on_progress = [en, &pc] (const download_progress& dp)
        {
          pc.update_progress (en, dp.downloaded_bytes, dp.total_bytes);
        };

        ts.emplace_back (t, en, &d);
      }

      if (ts.empty ())
        break;

      info ("executing download batch ({} queued files)", ts.size ());
      pc.start ();

      auto ml ([&io, &ts, &pc] () -> asio::awaitable<void>
      {
        // Periodically scan the active tasks to see if any have finished.
        //
        // If they have, we clean up their UI entries and mark them done. If a
        // task fails, we increment its retry counter so it will be picked up
        // again in the next outer loop iteration.
        //
        asio::steady_timer tm (io);
        while (!ts.empty ())
        {
          erase_if (ts,
                    [&pc] (const auto& at)
          {
            if (at.h->completed () || at.h->failed ())
            {
              at.h->on_progress = nullptr;
              pc.remove_entry (at.ui);

              if (at.h->completed ())
                at.ctx->done = true;
              else
                at.ctx->retries++;

              return true;
            }

            return false;
          });

          if (!ts.empty ())
          {
            tm.expires_after (chrono::milliseconds (25));
            co_await tm.async_wait (asio::use_awaitable);
          }
        }
      });

      // Run the downloads and the monitoring loop concurrently, yielding back
      // until everything completes.
      //
      // Notice that we use wait_for_all() to avoid orphaned operations running
      // in the background if an exception is thrown.
      //
      auto [order, err_dl, err_ui](
        co_await asio::experimental::make_parallel_group (
          asio::co_spawn (io, dc.execute_all (), asio::deferred),
          asio::co_spawn (io, ml (), asio::deferred))
          .async_wait (asio::experimental::wait_for_all (),
                       asio::use_awaitable));

      if (err_dl)
        std::rethrow_exception (err_dl);
      if (err_ui)
        std::rethrow_exception (err_ui);

      co_await pc.stop ();

      hp = ranges::any_of (ds, is_pd);
    }

    auto is_f ([] (const auto& d)
    {
      return !d.done;
    });

    ptrdiff_t fc (ranges::count_if (ds, is_f));

    if (fc > 0)
      throw runtime_error (std::to_string (fc) +
                           " downloads failed permanently after retries");

    info ("validating and applying staged files...");

    for (const auto& d : ds)
    {
      // Validate that the file actually ended up on disk and matches our
      // expected dimensions.
      //
      // A missing file or a size mismatch at this stage strongly implies a
      // truncated download or a local filesystem failure.
      //
      if (!exists (d.tmp, e))
        throw runtime_error ("downloaded file missing from staging area: " +
                             to_utf8 (d.tmp));

      uintmax_t fs (file_size (d.tmp, e));

      if (e || fs == 0 || (d.size > 0 && fs != d.size))
      {
        throw runtime_error ("downloaded file failed size validation: " +
                             to_utf8 (d.tmp));
      }

      if (d.dst.has_parent_path ())
      {
        path pd (weakly_canonical (d.dst.parent_path (), e));

        if (e)
          throw system_error (e,
                              "failed to canonicalize parent directory: " +
                                to_utf8 (pd));

        create_directories (pd, e);

        if (e)
          throw system_error (e,
                              "failed to create parent directory: " +
                                to_utf8 (pd));
      }

      if (exists (d.dst))
        remove (d.dst, e);

      rename (d.tmp, d.dst, e);

      // If a simple rename fails, it is usually because the staging directory
      // and the destination directory reside on different filesystem mounts. In
      // this case, the best we can do is attempt a full copy.
      //
      if (e)
      {
        copy_file (d.tmp, d.dst, copy_options::overwrite_existing, e);

        if (!e)
          remove (d.tmp, e);
        else
          throw runtime_error ("failed to apply file: " + to_utf8 (d.dst));
      }
    }

    unordered_map<string, const manifest_archive*> am;

    for (const auto& a : md.archives)
      am[to_utf8 (manifest_coordinator::resolve_path (a, ir))] = &a;

    for (const auto& d : ds)
    {
      // If the item is a zip file and matches a known archive in our manifest,
      // extract it directly into the root and track its contents.
      //
      if (d.dst.extension () == ".zip" || d.dst.extension () == ".ZIP")
      {
        auto i (am.find (to_utf8 (d.dst)));

        if (i != am.end ())
        {
          info ("extracting downloaded archive: {}", to_utf8 (d.dst));
          co_await manifest_coordinator::extract_archive (*i->second,
                                                          d.dst,
                                                          ir);

          auto resolve_p ([&ir] (const auto& s)
          {
            return manifest_coordinator::resolve_path (s, ir);
          });

          auto efs (i->second->files | views::transform (resolve_p) |
                    ranges::to<vector> ());

          cc.track (efs, d.comp, d.ver);
          remove (d.dst, e);

          continue;
        }
      }

      cc.track (to_utf8 (d.dst), d.comp, d.ver, d.hash);
    }
  }

  // Synchronize the core client.
  //
  asio::awaitable<void>
  sync_client (asio::io_context& io,
                github_coordinator& gh,
                download_coordinator& dc,
                progress_coordinator& pc,
                cache_coordinator& cc,
                const path& root,
                bool pre)
  {
    info ("synchronizing client component...");

    auto rel (co_await gh.fetch_latest_release ("iw4x", "iw4x-client", pre));
    bool out (cc.outdated (component_type::client, rel.tag_name));

    if (!out)
    {
      auto s (cc.audit (component_type::client));
      bool ok (ranges::all_of (s | views::values, [] (auto st) {
        return st == file_state::valid; }));

      if (ok)
      {
        info ("client components are valid and up to date");
        co_return;
      }

      warning ("client physical audit failed, forcing reconcile");
    }

    auto ms (co_await gh.fetch_manifest (rel));
    manifest m (ms);
    auto p (cc.plan (m, component_type::client, rel.tag_name));

    for (auto& i : p | views::filter ([] (const auto& x) {
      return x.action == reconcile_action::download && x.url.empty (); }))
    {
      string fn (to_utf8 (from_utf8 (i.path).filename ()));
      auto it (ranges::find_if (rel.assets, [&fn] (const auto& a) {
        return a.name == fn; }));

      if (it != rel.assets.end ())
        i.url = it->browser_download_url;

      if (i.url.empty ())
      {
        throw runtime_error (
          "reconciliation failed: unable to resolve URL for " +
          i.path);
      }
    }

    co_await execute_plan (io, dc, pc, cc, p, m, root);
    cc.clean (m, component_type::client);
    cc.stamp (component_type::client, rel.tag_name);
  }

  // Synchronize the rawfiles repository.
  //
  asio::awaitable<void>
  sync_rawfiles (asio::io_context& io,
                  github_coordinator& gh,
                  download_coordinator& dc,
                  progress_coordinator& pc,
                  cache_coordinator& cc,
                  const path& root,
                  bool pre)
  {
    info ("synchronizing rawfiles component...");

    auto rel (co_await gh.fetch_latest_release ("iw4x", "iw4x-rawfiles", pre));
    bool out (cc.outdated (component_type::rawfiles, rel.tag_name));

    if (!out)
    {
      auto s (cc.audit (component_type::rawfiles));
      bool ok (ranges::all_of (s | views::values, [] (auto st) {
        return st == file_state::valid; }));

      if (ok)
      {
        info ("rawfiles components are valid and up to date");
        co_return;
      }

      warning ("rawfiles physical audit failed, forcing reconcile");
    }

    auto ms (co_await gh.fetch_manifest (rel));
    manifest m (ms);

    for (auto& a : m.archives)
    {
      if (a.name == "__launcher_archive.zip")
        a.name = "release.zip";
    }

    auto p (cc.plan (m, component_type::rawfiles, rel.tag_name));

    for (auto& i : p | views::filter ([] (const auto& x) {
      return x.action == reconcile_action::download && x.url.empty (); }))
    {
      string fn (to_utf8 (from_utf8 (i.path).filename ()));

      auto it (ranges::find_if (rel.assets, [&fn] (const auto& a) {
        return a.name == fn; }));

      if (it == rel.assets.end ())
      {
        string asset_name = fn;
        replace (asset_name.begin (), asset_name.end (), '.', '_');
        asset_name = "__launcher_" + asset_name + ".bin";

        it = ranges::find_if (rel.assets, [&asset_name] (const auto& a) {
          return a.name == asset_name; });
      }

      if (it != rel.assets.end ())
        i.url = it->browser_download_url;

      if (i.url.empty ())
      {
        throw runtime_error (
          "reconciliation failed: unable to resolve URL for " +
          i.path);
      }
    }

    co_await execute_plan (io, dc, pc, cc, p, m, root);
    cc.clean (m, component_type::rawfiles);
    cc.stamp (component_type::rawfiles, rel.tag_name);
  }

  // Synchronize dynamic downloadable content (DLC).
  //
  asio::awaitable<void>
  sync_dlc (asio::io_context& io,
            http_coordinator& hc,
            download_coordinator& dc,
            progress_coordinator& pc,
            cache_coordinator& cc,
            const path& root)
  {
    info ("synchronizing dlc component...");

    auto ms (co_await hc.get ("https://cdn.iw4x.io/update.json"));
    manifest dlc (ms, manifest_format::dlc);

    manifest m;
    m.archives = dlc.files | views::transform ([] (const auto& f)
    {
      manifest_archive x;
      x.name = f.path;
      x.url = "https://cdn.iw4x.io/" + f.path;
      x.size = f.size;
      x.hash = f.hash;

      return x;
    }) | ranges::to<vector> ();

    auto p (cc.plan (m, component_type::dlc, "dlc"));

    for (auto& i : p | views::filter ([] (const auto& x) {
      return x.action == reconcile_action::download && x.url.empty (); }))
    {
      auto it (ranges::find_if (m.archives,
                                [&i, &root] (const auto& a)
      {
        return to_utf8 (from_utf8 (i.path)) ==
          to_utf8 (manifest_coordinator::resolve_path (a, root));
      }));

      if (it != m.archives.end ())
        i.url = it->url;

      if (i.url.empty ())
      {
        throw runtime_error (
          "reconciliation failed: unable to resolve URL for " + i.path);
      }
    }

    co_await execute_plan (io, dc, pc, cc, p, m, root);
    cc.clean (m, component_type::dlc);
    cc.stamp (component_type::dlc, "dlc");
  }

#ifdef __linux__
  asio::awaitable<void>
  sync_helper (asio::io_context& io,
                github_coordinator& gh,
                download_coordinator& dc,
                progress_coordinator& pc,
                cache_coordinator& cc,
                const path& root,
                bool pre)
  {
    info ("synchronizing linux steam helper component...");

    auto rel (co_await gh.fetch_latest_release ("iw4x", "launcher-steam", pre));
    bool out (cc.outdated (component_type::helper, rel.tag_name));

    if (!out)
    {
      auto s (cc.audit (component_type::helper));
      bool ok (ranges::all_of (s | views::values, [] (auto st) {
        return st == file_state::valid; }));

      if (ok)
      {
        info ("steam helper components are valid and up to date");
        co_return;
      }

      warning ("steam helper physical audit failed, forcing reconcile");
    }

    manifest m;
    m.archives = rel.assets
      | views::filter ([] (const auto& a) {
          return a.name == "steam.exe" || a.name == "steam_api64.dll"; })
      | views::transform ([] (const auto& a)
        {
          manifest_archive x;
          x.name = a.name;
          x.url = a.browser_download_url;
          x.size = a.size;

          return x;
        })
      | ranges::to<vector> ();

    auto p (cc.plan (m, component_type::helper, rel.tag_name));

    for (auto& i : p | views::filter ([] (const auto& x) {
      return x.action == reconcile_action::download && x.url.empty (); }))
    {
      string fn (to_utf8 (from_utf8 (i.path).filename ()));
      auto it (ranges::find_if (rel.assets, [&fn] (const auto& a) {
        return a.name == fn; }));

      if (it != rel.assets.end ())
        i.url = it->browser_download_url;

      if (i.url.empty ())
        throw runtime_error (
          "reconciliation failed: unable to resolve URL for " +
          i.path);
    }

    co_await execute_plan (io, dc, pc, cc, p, m, root);
    cc.clean (m, component_type::helper);
    cc.stamp (component_type::helper, rel.tag_name);
  }
#endif

#ifdef __linux__
  asio::awaitable<void>
  execute (asio::io_context& io,
            const path& root,
            const string& exe,
            const vector<string>& args)
  {
    if (exe.empty ())
      throw runtime_error ("game binary unspecified");

    path bin (root / from_utf8 (exe));
    error_code ec;

    bin = canonical (bin, ec);

    if (ec)
      throw system_error (ec,
                          "failed to canonicalize game binary path: " +
                            to_utf8 (bin));

    proton_coordinator proton (io);

    if (!exists (root / "steam.exe"))
      throw runtime_error ("runtime dependency missing: steam.exe");

    path steam_root;

    if (const char* h = getenv ("HOME"))
      steam_root = path (h) / ".steam" / "steam";

    info ("launching {} via proton", to_utf8 (bin));

    bool ok (co_await proton.complete_launch (steam_root, bin, 10190, args));

    if (!ok)
    {
      warning ("proton execution failed, attempting fallback to wine");

      path wine_exe (from_utf8 (process::search_path ("wine").string ()));

      if (wine_exe.empty ())
        throw runtime_error ("wine is not installed or not in PATH");

      wine_exe = canonical (wine_exe, ec);

      if (ec)
        throw system_error (ec,
                            "failed to canonicalize wine executable path: " +
                              to_utf8 (wine_exe));

      vector<string> wa;
      wa.push_back (to_utf8 (bin));
      wa.insert (wa.end (), args.begin (), args.end ());

      info ("launching via wine: {} {}", to_utf8 (wine_exe), to_utf8 (bin));

      process::spawn (wine_exe.string (),
                      process::args (wa),
                      process::start_dir (root.string ()));

      info ("wine game process spawned");
    }
    else
    {
      info ("proton execution initiated");
    }
  }
#else
  asio::awaitable<void>
  execute (asio::io_context& /* io */,
            const path& root,
            const string& exe,
            const vector<string>& args)
  {
    if (exe.empty ())
      throw runtime_error ("game binary unspecified");

    path bin (root / from_utf8 (exe));
    error_code ec;

    bin = canonical (bin, ec);

    if (ec)
      throw system_error (ec,
                          "failed to canonicalize game binary path: " +
                            to_utf8 (bin));

    info ("launching native game process: {}", to_utf8 (bin));

    // On Windows, boost::process heavily relies on standard strings mapping to
    // the ANSI codepage which corrupts Unicode strings (Russian, Chinese). We
    // construct wide strings (UTF-16) to force boost::process into
    // using CreateProcessW wide-character API.
    //
    vector<wstring> wa;
    wa.reserve (args.size ());

    for (const auto& a : args)
      wa.push_back (from_utf8 (a).wstring ());

    process::spawn (bin.wstring (),
                    process::args (wa),
                    process::start_dir (root.wstring ()));

    info ("native game process spawned");

    co_return;
  }
#endif
}

using namespace launcher;

int
main (int argc, char* argv[])
try
{
  active_logger = new logger;

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

    o << "usage: launcher[options]" << "\n"
      << "options:" << "\n";

    opt.print_usage (o);

    return 0;
  }

  reanchor_cwd ();

  asio::io_context io;

  if (!opt.no_self_update () || opt.self_update_only ())
  {
    auto pc (make_unique<progress_coordinator> (io));
    exception_ptr ex;

    asio::co_spawn (io,[&io, &opt, &pc] () -> asio::awaitable<void>
    {
      exception_ptr ep;

      try
      {
        co_await check_self_update (io,
                                    opt.prerelease (),
                                    opt.self_update_only (),
                                    *pc);
      }
      catch (...)
      {
        ep = current_exception ();
      }

      co_await pc->stop ();

      if (ep)
        rethrow_exception (ep);

    } (), [&io, &ex] (exception_ptr ep) { ex = ep; });

    io.restart ();
    io.run ();
    io.restart ();

    if (ex)
      rethrow_exception (ex);

    if (opt.self_update_only ())
      return 0;
  }

  // Determine the installation path. We use the current directory as the
  // isolating scope to differentiate multiple launcher installations.
  //
  path root;
  path cr (resolve_cache_root ());

  trace_l3 ("opening global cache database at {}", to_utf8 (cr));
  cache_database db (cr);

  string scope (path_digest (current_path ()));

  if (opt.path_specified ())
  {
    root = from_utf8 (opt.path ());
    info ("using CLI-specified install location: {}", to_utf8 (root));

    // Cache user-specified path in the database for future runs so the user
    // doesn't have to keep passing --path manually.
    //
    trace_l2 ("saving user-specified path override to database");
    db.setting (setting_keys::inst_path (scope), to_utf8 (root));
  }
  else
  {
    trace_l2 ("path not specified on CLI, checking database cache");
    string saved (db.setting_value (setting_keys::inst_path (scope)));

    if (!saved.empty ())
    {
      path p (from_utf8 (saved));

      if (exists (p))
      {
        info ("using previously saved install path override: {}", to_utf8 (p));
        root = p;
      }
      else
      {
        trace_l2 ("saved path override does not exist on disk, ignoring");
      }
    }
  }

  // If no explicit path override was found or cached, attempt to resolve via
  // the Steam installation heuristic. If we fail, we halt.
  //
  if (root.empty ())
  {
    trace_l2 (
      "no cached override found, attempting steam installation detection");

    exception_ptr ex;

    asio::co_spawn (io,
                    [&io, &root, &scope, &db] () -> asio::awaitable<void>
    {
      auto sp (co_await get_mw2_default_path (io));

      if (sp && exists (*sp))
      {
        info ("installation path: {}", to_utf8 (*sp));
        root = *sp;
      }
      else
      {
        throw runtime_error (
          "could not locate installation and no --path was specified");
      }
    } (), [&ex] (exception_ptr ep) { ex = ep; });

    io.restart ();
    io.run ();
    io.restart ();

    if (ex)
      rethrow_exception (ex);
  }

  error_code root_ec;

  root = canonical (root, root_ec);

  if (root_ec)
    throw system_error (root_ec,
                        "failed to canonicalize installation root: " +
                          to_utf8 (root));

  info ("final installation root resolved to: {}", to_utf8 (root));

  // Check if launcher version changed, if so wipe local .iw4x cache
  //
  {
    string current_ver (HELLO_VERSION_ID);
    string scope_ver_key ("launcher_version_" + path_digest (root));
    string saved_ver (db.setting_value (scope_ver_key));

    if (saved_ver != current_ver)
    {
      error_code ec;
      path cache_dir (root / ".iw4x");

      if (exists (cache_dir, ec))
      {
        info (
          "launcher version changed ({} -> {}), wiping local cache directory: "
          "{}",
          saved_ver.empty () ? "none" : saved_ver,
          current_ver,
          to_utf8 (cache_dir));

        remove_all (cache_dir, ec);

        if (ec)
          warning ("failed to remove cache directory: {}", ec.message ());
      }

      db.setting (scope_ver_key, current_ver);
    }
  }

  github_coordinator   gh (io);
  http_coordinator     hc (io);
  download_coordinator dc (io, opt.jobs ());
  progress_coordinator pc (io);
  cache_coordinator    cc (io, root);

  cc.set_github_coordinator (&gh);
  cc.set_download_coordinator (&dc);
  cc.set_progress_coordinator (&pc);

  // Visually alert the user if we hit GitHub's unauthenticated API rate limits
  //
  bind_rate_limit_ui (io, gh, pc);

  exception_ptr sync_ex;

  asio::co_spawn (
    io,
    [&io, &gh, &hc, &dc, &pc, &cc, &root, &opt] () -> asio::awaitable<void>
  {
    co_await sync_client (io, gh, dc, pc, cc, root, opt.prerelease ());
    co_await sync_rawfiles (io, gh, dc, pc, cc, root, opt.prerelease ());
    co_await sync_dlc (io, hc, dc, pc, cc, root);

#ifdef __linux__
    co_await sync_helper (io, gh, dc, pc, cc, root, true);
#endif
  }(),
    [&io, &sync_ex] (exception_ptr ep)
  {
    sync_ex = ep;
    io.stop ();
  });

  io.restart ();
  io.run ();

  if (sync_ex)
    rethrow_exception (sync_ex);

  info ("all components synchronized and up to date");

  if (opt.skip_launch ())
  {
    info ("updates completed, skipping game launch as requested and exiting");
    return 0;
  }

  exception_ptr exec_ex;

  asio::co_spawn (
    io,
    [&io, &root, &opt] () -> asio::awaitable<void>
  {
    co_await execute (io, root, opt.game_exe (), opt.game_args ());

  } (), [&io, &exec_ex] (exception_ptr ep) { exec_ex = ep; io.stop (); });

  io.restart ();
  io.run ();

  if (exec_ex)
    rethrow_exception (exec_ex);

  info ("execution payload dispatched, terminating launcher");

  return 0;
}
catch (const cli::exception& ex)
{
  error ("CLI exception caught in main: {}", ex.what ());
  return 1;
}
catch (const exception& ex)
{
  error ("exception caught in main: {}", ex.what ());
  return 1;
}
catch (...)
{
  error ("You've run into a launcher bug we didn't see coming. Since this isn't"
         "something a restart can fix, please report it to our team so we can"
         "investigate.");
  return 1;
}
