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
#include <launcher/launcher-update.hxx>

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
      progress_.start ();

      // Discovery Phase.
      //
      // We resolve the remote state of all components (Client, Rawfiles,
      // Helper, and DLC) in parallel to minimize latency. We don't log here yet
      // because we want the first output to be the result ("Up to date" or
      // "Update available").
      //
      remote_state remote (co_await resolve_remote_state ());

      co_await reconcile_artifacts (remote);
      co_await progress_.stop ();
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

    // Synchronize local filesystem with the remote manifest.
    //
    asio::awaitable<void>
    reconcile_artifacts (const remote_state& remote)
    {
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
      inject_assets (remote.helper.assets, "steam_api64.dll");
#endif

      int raw_count (inject_assets (remote.raw.assets));

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
      }

      // Collect all files and archives to download.
      //
      // We filter out files that are part of archives (they'll be extracted
      // from the archive after download).
      //
      vector<manifest_file> files_to_download;
      for (const auto& f : m.files)
      {
        if (!f.archive_name)
          files_to_download.push_back (f);
      }

      const auto& archives_to_download (m.archives);

      if (files_to_download.empty () && archives_to_download.empty ())
        co_return;

      // Queue acquisition tasks.
      //
      uint64_t total_bytes (0);
      for (const auto& f : files_to_download) total_bytes += f.size;
      for (const auto& a : archives_to_download) total_bytes += a.size;

      unordered_map<shared_ptr<download_coordinator::task_type>,
                    shared_ptr<progress_entry>> task_map;

      auto schedule_download = [&] (const string& url,
                                    const fs::path& target,
                                    const string& name,
                                    uint64_t size)
      {
        download_request req;
        req.urls.push_back (url);
        req.target = target;
        req.name = name;
        req.expected_size = size;

        if (url.find ("cdn.iw4x.io") != string::npos)
          req.rate_limit_bytes_per_second = 2097152; // 2 MB/s

        auto task (downloads_.queue_download (move (req)));
        auto entry (progress_.add_entry (name));

        task_map[task] = entry;
        entry->metrics ().total_bytes.store (size, memory_order_relaxed);
        task->on_progress = [entry, this] (const download_progress& p)
        {
          progress_.update_progress (entry,
                                     p.downloaded_bytes,
                                     p.total_bytes);
        };
      };

      for (const auto& f : files_to_download)
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
                           f.size);
      }

      for (const auto& a : archives_to_download)
      {
        if (a.url.empty ()) continue;

        fs::path t (
          manifest_coordinator::resolve_path (a, ctx_.install_location));

        if (t.has_parent_path ())
          fs::create_directories (t.parent_path ());

        schedule_download (a.url, t, a.name, a.size);
      }

      asio::co_spawn (ioc_, downloads_.execute_all (), asio::detached);

      // Drain the queue.
      //
      while (downloads_.completed_count () + downloads_.failed_count () <
             downloads_.total_count ())
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
      for (const auto& a : archives_to_download)
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
              ctx_.install_location);

            fs::remove (p);
          }
          catch (const exception& e)
          {
            throw runtime_error ("extraction failure: " + a.name + ": " + e.what ());
          }
        }
      }
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
          bp::start_dir (ctx_.install_location.string ())
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
      // If we are running with the UI, pop up a dialog with the countdown so
      // the user knows exactly why we are stalled.
      //
      string s (
        msg + "\n\nTime remaining: " + std::to_string (rem) + " seconds");

      progress_.show_dialog ("Rate Limit", s);

      // If the countdown has finished, we can dismiss the dialog and
      // carry on.
      //
      if (rem == 0)
        progress_.hide_dialog ();
    }

    asio::io_context& ioc_;
    runtime_context ctx_;
    github_coordinator github_;
    http_coordinator http_;
    download_coordinator downloads_;
    progress_coordinator progress_;
  };

  // Resolve MW2 installation root via Steam.
  //
  // If we find a Steam installation, prompt the user to confirm whether they
  // want to use it.
  //
  asio::awaitable<optional<fs::path>>
  resolve_install_root (asio::io_context& ioc)
  {
    try
    {
      auto p (co_await get_mw2_default_path (ioc));

      if (p && fs::exists (*p))
      {
        cout
          << "Found Steam installation of Call of Duty: Modern Warfare 2:\n"
          << "  " << p->string () << "\n\n";

        bool accepted (
          confirm_action (
            "Install IW4x to this directory? [Y/n] "
            "(n = use current directory)", 'y'));

        if (accepted)
          co_return p;
      }
    }
    catch (const exception&)
    {
      // Silently fall back to the current directory if detection fails.
      //
    }

    co_return nullopt;
  }

  // Check for and optionally install launcher updates.
  //
  asio::awaitable<bool>
  check_self_update (asio::io_context& io,
                     bool hl,
                     bool pre,
                     progress_coordinator* pc = nullptr)
  {
    auto uc (make_update_coordinator (io));
    uc->set_headless (hl);
    uc->set_include_prerelease (pre);

    if (pc != nullptr)
      uc->set_progress_coordinator (pc);

    try
    {
      auto s (co_await uc->check_and_update ());

      // Return true if we are in the middle of a restart (meaning we
      // shouldn't continue with the calling logic).
      //
      if (uc->state () == update_state::restarting)
        co_return true;

      co_return false;
    }
    catch (const exception& e)
    {
      // Warn and move on if update check fails.
      //
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

    // Self-update check.
    //
    {
      bool r (false);

      // Setup the progress feedback if we are not in the headless mode.
      //
      auto pc (make_unique<progress_coordinator> (ioc));

      asio::co_spawn (
        ioc,
        check_self_update (ioc,
                           opt.prerelease (),
                           pc.get ()),
        [&r, &ioc, &pc] (exception_ptr e, bool v)
        {
          if (!e)
            r = v;

          if (pc != nullptr)
            asio::co_spawn (ioc, pc->stop (), asio::detached);

          ioc.stop ();
        });

      if (pc != nullptr)
        pc->start ();

      ioc.restart ();
      ioc.run ();
      ioc.restart ();

      // If we applied an update that requires a restart (e.g. replaced the
      // executable), we are done.
      //
      if (r)
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
