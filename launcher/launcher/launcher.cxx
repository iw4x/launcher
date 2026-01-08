#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <cctype>

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <launcher/version.hxx>
#include <launcher/launcher-http.hxx>
#include <launcher/launcher-steam.hxx>
#include <launcher/launcher-github.hxx>
#include <launcher/launcher-options.hxx>
#include <launcher/launcher-download.hxx>
#include <launcher/launcher-manifest.hxx>
#include <launcher/launcher-progress.hxx>

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
  yn_prompt (const string& prompt, char def = '\0')
  {
    string a;
    do
    {
      cout << prompt << ' ';

      // Note: getline() sets the failbit if it fails to extract anything,
      // and the eofbit if it hits EOF before the delimiter.
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
        // actual newline from the user to confirm they are present and
        // paying attention.
        //
        if (!e)
          a = def;
      }
    } while (a != "y" && a != "Y" && a != "n" && a != "N");

    return a == "y" || a == "Y";
  }

  // Calculate a unique, filesystem-safe key for a given path.
  //
  // We need to associate metadata (like the "accepted" state) with specific
  // installation paths. Since paths can contain characters that are invalid
  // for filenames (separators, etc.), hashing the string gives us a stable,
  // safe identifier.
  //
  static string
  path_key (const fs::path& p)
  {
    std::hash<string> h;
    return std::to_string (h (p.string ()));
  }

  // Determine the directory for user preference and state caching.
  //
  // We try to be good citizens by respecting platform conventions (XDG on
  // Linux, AppData on Windows). However, if we can't create directories
  // there, we fall back to a local ".launcher-cache" in the current working
  // directory to avoid crashing.
  //
  static fs::path
  get_cache_dir (const fs::path& install_path = {})
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

    // If we are looking for the cache specific to an installation, append
    // its unique key.
    //
    if (!install_path.empty ())
      d /= path_key (install_path);

    error_code ec;
    fs::create_directories (d, ec);

    if (ec)
    {
      // Fallback: If we can't write to the system location, try a local
      // directory.
      //
      d = fs::current_path () / ".launcher-cache";

      if (!install_path.empty ())
        d /= path_key (install_path);

      fs::create_directories (d, ec);
    }

    return d;
  }

  // Attempt to detect a default Modern Warfare 2 installation path.
  //
  // We rely on the Steam library logic to find the game. If found, we check
  // our local cache to see if the user has previously made a decision about
  // this path to avoid pestering them on every run.
  //
  asio::awaitable<optional<fs::path>>
  detect_default_path ()
  {
    try
    {
      auto executor (co_await asio::this_coro::executor);
      auto& ioc (static_cast<asio::io_context&> (executor.context ()));

      auto p (co_await get_mw2_default_path (ioc));

      if (p && fs::exists (*p))
      {
        fs::path cd (get_cache_dir ());
        string key (path_key (*p));

        fs::path yes_marker (cd / (key + ".yes"));
        fs::path no_marker (cd / (key + ".no"));

        bool is_yes (fs::exists (yes_marker));
        bool is_no (fs::exists (no_marker));

        // If we have both markers, the cache is inconsistent (maybe user
        // fiddling or a race condition). Better to wipe both and ask again.
        //
        if (is_yes && is_no)
        {
          fs::remove (yes_marker);
          fs::remove (no_marker);
          is_yes = false;
          is_no = false;
        }

        if (is_yes || is_no)
        {
          cout << "Found Steam installation of Call of Duty: Modern Warfare 2:\n"
               << "  " << p->string () << "\n"
               << "  (Using cached preference: "
               << (is_yes ? "yes" : "no") << ")\n\n";

          if (is_yes)
            co_return p;
        }
        else
        {
          cout << "Found Steam installation of Call of Duty: Modern Warfare 2:\n"
               << "  " << p->string () << "\n\n";

          // Ask the user if they want to use this path.
          //
          bool answer (yn_prompt ("Install IW4x to this directory? [Y/n] "
                                  "(n = use current directory)", 'y'));

          // Cache the answer by touching a marker file.
          //
          {
            ofstream os (answer ? yes_marker : no_marker);
          }

          if (answer)
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

  // Launcher configuration.
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
      // Prepare the cache structure.
      //
      fs::path cache_dir (get_cache_dir (conf.install_path));
      fs::path installed_marker (cache_dir / ".launcher-installed");
      fs::path version_client_marker (cache_dir / ".launcher-version-client");
      fs::path version_raw_marker (cache_dir / ".launcher-version-raw");
      fs::path archive_cache_file (cache_dir / ".launcher-archive.json");

      github_coordinator   github (ioc);
      http_coordinator     http (ioc);
      download_coordinator downloads (ioc, conf.max_parallel);
      progress_coordinator progress (ioc);

      if (!conf.no_progress)
        progress.start ();

      // Helper to log either to stdout or the TUI.
      //
      auto log = [&] (const string& msg)
      {
        if (conf.no_progress)
          cout << msg << "\n";
        else
          progress.add_log (msg);
      };

      log ("Checking for updates (" + conf.owner + "/" + conf.repo + ")...");

      // We fetch both the client and the rawfiles releases in parallel.
      //
      auto [client_rel, raw_rel] = co_await (
        github.fetch_latest_release (conf.owner,
                                     conf.repo,
                                     conf.prerelease) &&
        github.fetch_latest_release ("iw4x",
                                     "iw4x-rawfiles",
                                     conf.prerelease)
      );

      // Check if we are already up to date by comparing tags.
      //
      bool cache_valid (fs::exists (installed_marker) &&
                        fs::exists (version_client_marker) &&
                        fs::exists (version_raw_marker));

      if (cache_valid && !conf.force_update)
      {
        ifstream ifs_c (version_client_marker);
        string local_client_tag;
        getline (ifs_c, local_client_tag);

        ifstream ifs_r (version_raw_marker);
        string local_raw_tag;
        getline (ifs_r, local_raw_tag);

        if (local_client_tag == client_rel.tag_name &&
            local_raw_tag == raw_rel.tag_name)
        {
          if (conf.no_progress)
            cout << "Client is up to date (" << local_client_tag << ")." << "\n";
          else
            co_await progress.stop ();

          co_return 0;
        }
      }

      log ("Update available or verifying installation...");
      log ("Client: " + client_rel.tag_name);
      log ("Rawfiles: " + raw_rel.tag_name);

      archive_cache cache (archive_cache_file);
      try
      {
        cache.load ();
      }
      catch (const exception& e)
      {
        // Not fatal, we'll just re-verify archives if the cache is corrupt.
        //
        if (conf.no_progress)
          cerr << "warning: failed to load archive cache: " << e.what () << "\n";
      }

      log ("Downloading manifest...");

      manifest m (co_await github.fetch_manifest (client_rel));

      log ("Manifest loaded: " +
           std::to_string (manifest_coordinator::get_file_count (m)) + " files");

      // Inject the rawfiles assets into the manifest as archives.
      //
      for (const auto& a : raw_rel.assets)
      {
        manifest_archive ma;
        ma.name = a.name;
        ma.url = a.browser_download_url;
        ma.size = a.size;
        m.archives.push_back (move (ma));
      }

      log ("Added " + std::to_string (raw_rel.assets.size ()) + " rawfiles");

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
          conf.install_path,
          &cache));

      if (missing_files.empty () && missing_archives.empty ())
      {
        // Everything looks good, just update the markers.
        //
        { ofstream os (version_client_marker); os << client_rel.tag_name; }
        { ofstream os (version_raw_marker);    os << raw_rel.tag_name; }
        { ofstream os (installed_marker); }

        if (conf.no_progress)
          cout << "All files up to date." << "\n";
        else
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

      // Helper lambda.
      //
      auto queue_item = [&] (const string& url,
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
          req.verification_method = conf.disable_checksum
            ? download_verification::none
            : download_verification::sha256;
          req.verification_value = hash.string ();
        }
        else
        {
          req.verification_method = download_verification::none;
        }

        auto task (downloads.queue_download (move (req)));

        if (!conf.no_progress)
        {
          auto entry (progress.add_entry (name));
          task_entries[task] = entry;
          entry->metrics ().total_bytes.store (size, memory_order_relaxed);

          task->on_progress = [entry, &progress] (const download_progress& p)
          {
            progress.update_progress (entry, p.downloaded_bytes, p.total_bytes);
          };
        }
      };

      for (const auto& f : missing_files)
      {
        if (!f.asset_name)
          continue;

        auto a (github.find_asset (client_rel, *f.asset_name));

        if (!a)
        {
          cerr << "warning: asset not found for file: " << f.path << "\n";
          continue;
        }

        fs::path target (
          manifest_coordinator::resolve_path (f, conf.install_path));

        if (target.has_parent_path ())
          fs::create_directories (target.parent_path ());

        queue_item (a->browser_download_url,
                    target,
                    target.filename ().string (),
                    f.size,
                    f.hash);
      }

      for (const auto& a : missing_archives)
      {
        if (a.url.empty ())
          continue;

        fs::path target (
          manifest_coordinator::resolve_path (a, conf.install_path));

        if (target.has_parent_path ())
          fs::create_directories (target.parent_path ());

        queue_item (a.url, target, a.name, a.size, a.hash);
      }

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
          for (auto it (task_entries.begin ()); it != task_entries.end (); )
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

      // Post-process: Extract downloaded archives.
      //
      // Only .zip files need extraction; .iwd and .ff files are already in
      // their final form.
      //
      for (const auto& a : missing_archives)
      {
        // Skip non-zip archives (.iwd, .ff files don't need extraction).
        //
        fs::path p (a.name);
        string ext (p.extension ().string ());
        transform (ext.begin (), ext.end (), ext.begin (),
                   [] (unsigned char c) { return tolower (c); });

        if (ext != ".zip")
          continue;

        fs::path archive_path (
          manifest_coordinator::resolve_path (a, conf.install_path));

        if (fs::exists (archive_path))
        {
          try
          {
            co_await manifest_coordinator::extract_archive (a,
                                                            archive_path,
                                                            conf.install_path,
                                                            &cache);
            fs::remove (archive_path);
          }
          catch (const exception& e)
          {
            cerr << "error: failed to extract " << a.name << ": "
                 << e.what () << "\n";
            co_return 1;
          }
        }
      }

      try
      {
        cache.save ();
      }
      catch (const exception& e)
      {
        if (conf.no_progress)
          cerr << "warning: failed to cache archive: " << e.what () << "\n";
      }

      { ofstream os (installed_marker); }
      { ofstream os (version_client_marker); os << client_rel.tag_name; }
      { ofstream os (version_raw_marker);    os << raw_rel.tag_name; }

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
        << "launcher.name = [string] launcher"                               << "\n"
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

    launcher_config conf;

    // Set up the IO context first so we can run async operations during the
    // configuration phase (e.g., detecting Steam path).
    //
    asio::io_context ioc;
    int result (0);

    // Determine the installation path.
    //
    if (opt.path_specified ())
    {
      conf.install_path = fs::path (opt.path ());
    }
    else
    {
      // Try to locate MW2 via Steam.
      //
      optional<fs::path> detected_path;

      asio::co_spawn (
        ioc,
        detect_default_path (),
        [&detected_path, &ioc] (exception_ptr ex, optional<fs::path> p)
        {
          if (!ex)
            detected_path = p;
          ioc.stop ();
        });

      // Run the detection loop. We restart the context afterwards because
      // stop() makes it return, and we need it fresh for the launcher itself.
      //
      ioc.restart ();
      ioc.run ();
      ioc.restart ();

      conf.install_path = detected_path.value_or (fs::current_path ());
    }

    conf.owner = "iw4x";
    conf.repo = "iw4x-client";
    conf.prerelease = opt.prerelease ();
    conf.force_update = opt.force_update ();
    conf.no_progress = opt.no_ui ();
    conf.disable_checksum = opt.disable_checksum ();
    conf.max_parallel = opt.jobs ();

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
