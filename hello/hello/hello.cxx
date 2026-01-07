#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <hello/hello-options.hxx>
#include <hello/version.hxx>
#include <hello/hello-github.hxx>
#include <hello/hello-http.hxx>
#include <hello/hello-download.hxx>
#include <hello/hello-manifest.hxx>
#include <hello/hello-progress.hxx>

using namespace std;
namespace fs = filesystem;
namespace asio = boost::asio;

namespace hello
{
  // Launcher configuration options.
  //
  struct launcher_config
  {
    fs::path install_path;
    string owner;
    string repo;
    bool prerelease;
    bool force_update;
    bool no_progress;
    bool disable_checksum;
    size_t max_parallel;
  };

  // Main launcher logic.
  //
  // Here we coordinate the entire update process: starting with fetching the
  // release information from GitHub, then resolving the manifest against what
  // we have on the disk, and finally queuing the necessary downloads.
  //
  asio::awaitable<int>
  run_launcher (launcher_config conf)
  {
    auto executor (co_await asio::this_coro::executor);
    auto& ioc (static_cast<asio::io_context&> (executor.context ()));

    try
    {
      // Initialize the various coordinators we will need.
      //
      github_coordinator   github (ioc);
      http_coordinator     http (ioc);
      download_coordinator downloads (ioc, conf.max_parallel);
      progress_coordinator progress (ioc);

      // If the UI is enabled, we start the progress reporting now so the
      // user knows we are busy.
      //
      if (!conf.no_progress)
        progress.start ();

      // Helper to log messages to either cout or the progress UI.
      //
      auto log = [&] (const string& msg)
      {
        if (conf.no_progress)
          cout << msg << "\n";
        else
          progress.add_log (msg);
      };

      // First, we need to fetch the latest release metadata from GitHub to
      // see what version we should be running.
      //
      log ("Fetching release from " + conf.owner + "/" + conf.repo + "...");

      github_release release (
        co_await github.fetch_latest_release (conf.owner,
                                              conf.repo,
                                              conf.prerelease));

      log ("Found release: " + release.tag_name);

      // Now we download and parse the manifest asset associated with this
      // release.
      //
      log ("Downloading manifest...");

      manifest m (co_await github.fetch_manifest (release));

      log ("Manifest loaded: " +
           std::to_string (manifest_coordinator::get_file_count (m)) + " files");

      // Next, we handle the rawfiles. These are published as separate release
      // assets in the iw4x-rawfiles repository. We treat them as archives
      // that we will download and extract implicitly.
      //
      log ("Fetching rawfiles release...");

      github_release rawfiles (
        co_await github.fetch_latest_release ("iw4x",
                                              "iw4x-rawfiles",
                                              conf.prerelease));

      for (const auto& a : rawfiles.assets)
      {
        manifest_archive ma;
        ma.name = a.name;
        ma.url = a.browser_download_url;
        ma.size = a.size;
        m.archives.push_back (move (ma));
      }

      log ("Added " + std::to_string (rawfiles.assets.size ()) + " rawfiles");

      // We also need to fetch the DLC manifest from the CDN.
      //
      log ("Fetching DLC manifest...");

      string dlc_json (co_await http.get ("https://cdn.iw4x.io/update.json"));

      if (!dlc_json.empty ())
      {
        manifest dlc (dlc_json, manifest_format::dlc);

        // Convert the DLC files into archives for the downloader.
        //
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

        log ("Added " + std::to_string (dlc.files.size ()) + " DLC files");
      }

      // With the full manifest built, we can now diff it against the local
      // filesystem to see what's missing or outdated.
      //
      log ("Checking local files...");

      vector<manifest_file> missing_files (
        manifest_coordinator::get_missing_files (
          m,
          conf.install_path,
          false));

      vector<manifest_archive> missing_archives (
        manifest_coordinator::get_missing_archives (
          m,
          conf.install_path));

      if (missing_files.empty () && missing_archives.empty ())
      {
        if (conf.no_progress)
          cout << "All files up to date.\n";

        if (!conf.no_progress)
          co_await progress.stop ();

        co_return 0;
      }

      // Calculate the total download size so we can display it to the user.
      //
      uint64_t total_size (0);
      for (const auto& f : missing_files)
        total_size += f.size;
      for (const auto& a : missing_archives)
        total_size += a.size;

      if (conf.no_progress)
        cout << "Need to download "
             << (missing_files.size () + missing_archives.size ()) << " items ("
             << format_bytes (total_size) << ")\n";

      // Time to queue the actual downloads.
      //

      // We map tasks to progress entries so we can clean them up from the UI
      // once they are complete.
      //
      unordered_map<shared_ptr<download_coordinator::task_type>,
                    shared_ptr<progress_entry>> task_entries;

      // Queue the regular file downloads first.
      //
      for (const auto& f : missing_files)
      {
        if (!f.asset_name)
          continue;

        optional<github_asset> a (github.find_asset (release, *f.asset_name));

        if (!a)
        {
          cerr << "warning: asset not found for file: " << f.path << "\n";
          continue;
        }

        fs::path target (
          manifest_coordinator::resolve_path (f, conf.install_path));

        if (target.has_parent_path ())
          fs::create_directories (target.parent_path ());

        download_request req;
        req.urls.push_back (a->browser_download_url);
        req.target = target;
        req.name = target.filename ().string ();
        req.expected_size = f.size;
        req.verification_method = conf.disable_checksum
          ? download_verification::none
          : download_verification::sha256;
        req.verification_value = f.hash.value;

        string task_name (req.name);
        auto task (downloads.queue_download (move (req)));

        if (!conf.no_progress)
        {
          auto entry (progress.add_entry (task_name));
          task_entries [task] = entry;

          // Initialize the expected size so the progress bar scales
          // correctly from the start.
          //
          entry->metrics ().total_bytes.store (f.size, memory_order_relaxed);

          task->on_progress = [entry, &progress] (const download_progress& p)
          {
            progress.update_progress (entry, p.downloaded_bytes, p.total_bytes);
          };
        }
      }

        // Queue the archive downloads.
        //
        for (const auto& a : missing_archives)
        {
          if (a.url.empty ())
            continue;

          fs::path target (
            manifest_coordinator::resolve_path (a, conf.install_path));

          if (target.has_parent_path ())
            fs::create_directories (target.parent_path ());

          download_request req;
          req.urls.push_back (a.url);
          req.target = target;
          req.name = a.name;
          req.expected_size = a.size;
          req.verification_method = conf.disable_checksum
            ? download_verification::none
            : download_verification::sha256;
          req.verification_value = a.hash.value;

          string task_name (req.name);
          auto task (downloads.queue_download (move (req)));

          if (!conf.no_progress)
          {
            auto entry (progress.add_entry (task_name));
            task_entries [task] = entry;

            entry->metrics ().total_bytes.store (a.size, memory_order_relaxed);

            task->on_progress = [entry, &progress] (const download_progress& p)
            {
              progress.update_progress (entry,
                                        p.downloaded_bytes,
                                        p.total_bytes);
            };
          }
        }

      // Start the download engine.
      //
      if (conf.no_progress)
        cout << "Starting downloads...\n";

      asio::co_spawn (ioc, downloads.execute_all (), asio::detached);

      // Wait for completion, updating the UI as we go.
      //
      while (downloads.completed_count () + downloads.failed_count () <
             downloads.total_count ())
      {
        // Prune completed entries from the UI to keep it clean.
        //
        if (!conf.no_progress)
        {
          for (auto it (task_entries.begin ()); it != task_entries.end ();)
          {
            if (it->first->completed () || it->first->failed ())
            {
              progress.remove_entry (it->second);
              it = task_entries.erase (it);
            }
            else
            {
              ++it;
            }
          }
        }

        asio::steady_timer timer (ioc, chrono::milliseconds (100));
        co_await timer.async_wait (asio::use_awaitable);
      }

      // Finally, check if we had any failures.
      //
      size_t failed (downloads.failed_count ());

      if (failed > 0)
      {
        cerr << "error: " << failed << " download(s) failed\n";

        if (!conf.no_progress)
          co_await progress.stop ();

        co_return 1;
      }

      if (!conf.no_progress)
        co_await progress.stop ();

      co_return 0;
    }
    catch (const exception& e)
    {
      cerr << "error: " << e.what () << "\n";
      co_return 1;
    }
  }
}

int
main (int argc, char* argv[])
{
  using namespace std;
  using namespace hello;

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
      o << "# build2 buildfile hello"                                  << "\n"
        << "export.metadata = 1 hello"                                 << "\n"
        << "hello.name = [string] hello"                               << "\n"
        << "hello.version = [string] '"  << HELLO_VERSION_FULL << '\'' << "\n"
        << "hello.checksum = [string] '" << HELLO_VERSION_FULL << '\'' << "\n";

      return 0;
    }

    // Handle --version.
    //
    if (opt.version ())
    {
      auto& o (cout);

      o << "Hello " << HELLO_VERSION_ID << "\n";

      return 0;
    }

    // Handle --help.
    //
    if (opt.help ())
    {
      auto& o (cout);

      o << "usage: hello [options]"   << "\n"
        << "options:"                 << "\n";

      opt.print_usage (o);

      return 0;
    }

    // Build the launcher configuration from the parsed options.
    //
    launcher_config conf;
    conf.install_path = opt.path_specified ()
      ? fs::path (opt.path ())
      : fs::current_path ();

    conf.owner = "iw4x";
    conf.repo = "iw4x-client";
    conf.prerelease = false;
    conf.force_update = false;
    conf.no_progress = opt.no_ui ();
    conf.disable_checksum = true;
    conf.max_parallel = 4;

    // Set up the IO context and run the launcher loop.
    //
    asio::io_context ioc;
    int result (0);

    asio::co_spawn (
      ioc,
      run_launcher (conf),
      [&result, &ioc] (exception_ptr ex, int r)
      {
        result = r;

        if (ex)
        {
          try
          {
            rethrow_exception (ex);
          }
          catch (const exception& e)
          {
            cerr << "error: " << e.what () << "\n";
            result = 1;
          }
        }

        ioc.stop ();
      });

    ioc.run ();

    return result;
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
