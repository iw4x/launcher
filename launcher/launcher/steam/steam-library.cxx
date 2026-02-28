#include <launcher/steam/steam-library.hxx>

#include <fstream>
#include <vector>
#include <cstdlib>
#include <algorithm>

#include <boost/asio/post.hpp>
#include <boost/asio/co_spawn.hpp>

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>
#else
#  include <unistd.h>
#  include <pwd.h>
#endif

#include <launcher/launcher-log.hxx>

using namespace std;

namespace launcher
{
#ifdef _WIN32
  // Detect if we're running under Wine by checking for wine_get_version in
  // ntdll.dll.
  //
  static bool
  is_wine ()
  {
    static int r (-1); // Cache: -1 = unknown, 0 = no, 1 = yes.

    if (r == -1)
    {
      log::trace_l2 (categories::steam{}, "checking for wine environment via ntdll.dll");
      HMODULE ntdll (GetModuleHandleA ("ntdll.dll"));

      if (ntdll != nullptr)
      {
        auto proc (GetProcAddress (ntdll, "wine_get_version"));
        r = (proc != nullptr) ? 1 : 0;
        log::trace_l3 (categories::steam{}, "wine_get_version proc address: {}", (void*)proc);
      }
      else
      {
        log::warning (categories::steam{}, "failed to get handle for ntdll.dll");
        r = 0;
      }

      if (r == 1)
        log::info (categories::steam{}, "detected Wine environment");
      else
        log::trace_l2 (categories::steam{}, "native Windows environment detected");
    }

    return r == 1;
  }
#endif

  steam_library_manager::
  steam_library_manager (asio::io_context& ioc)
    : ioc_ (ioc),
      libraries_loaded_ (false)
  {
    log::trace_l2 (categories::steam{}, "initialized steam_library_manager");
  }

  // Detect the main Steam installation path.
  //
  // We delegate this to platform-specific implementations. Note that we cache
  // the result in steam_path_ so we don't have to re-scan the registry or
  // filesystem on subsequent calls.
  //
  asio::awaitable<optional<fs::path>> steam_library_manager::
  detect_steam_path ()
  {
    log::trace_l1 (categories::steam{}, "detecting main steam installation path");

    if (steam_path_)
    {
      log::trace_l2 (categories::steam{}, "returning cached steam path: {}", steam_path_->string ());
      co_return steam_path_;
    }

    optional<fs::path> r;

#ifdef _WIN32
    // If we're running under Wine, use Linux detection since Steam is likely
    // installed on the host Linux system.
    //
    if (is_wine ())
    {
      log::trace_l2 (categories::steam{}, "routing to linux steam path detection due to wine");
      r = co_await detect_steam_path_linux ();
    }
    else
    {
      log::trace_l2 (categories::steam{}, "routing to windows steam path detection");
      r = co_await detect_steam_path_windows ();
    }
#elif defined(__APPLE__)
    log::trace_l2 (categories::steam{}, "routing to macos steam path detection");
    r = co_await detect_steam_path_macos ();
#else
    log::trace_l2 (categories::steam{}, "routing to linux steam path detection");
    r = co_await detect_steam_path_linux ();
#endif

    if (r)
    {
      steam_path_ = *r;
      log::info (categories::steam{}, "detected main steam root: {}", r->string ());
    }
    else
    {
      log::warning (categories::steam{}, "failed to detect main steam path on this system");
    }

    co_return r;
  }

  // Linux detection logic.
  //
  // On Linux, Steam is typically installed in the user's home directory,
  // either under .steam or .local. However, we also need to check system-wide
  // locations and the Flatpak sandbox data directory.
  //
  asio::awaitable<optional<fs::path>> steam_library_manager::
  detect_steam_path_linux ()
  {
    vector<fs::path> candidates;

    const char* home (getenv ("HOME"));
    log::trace_l3 (categories::steam{}, "env HOME: {}", home ? home : "<null>");
    fs::path h (home ? home : "");

    // If we came up empty, this might be Wine.
    //
    if (h.empty ())
    {
      log::trace_l2 (categories::steam{}, "HOME environment variable empty, attempting fallbacks");
    #ifdef _WIN32
      // Try to construct the path based on the username. Prefer USER but fall
      // back to USERNAME which is standard on Windows.
      //
      const char* u (getenv ("USER"));
      if (u == nullptr)
        u = getenv ("USERNAME");

      log::trace_l3 (categories::steam{}, "env USER/USERNAME: {}", u ? u : "<null>");

      // Z:\home\<user>.
      //
      if (u != nullptr)
      {
        fs::path p ("Z:\\home");
        p /= u;

        log::trace_l3 (categories::steam{}, "checking fallback wine home directory: {}", p.string ());
        if (fs::exists (p))
        {
          log::debug (categories::steam{}, "resolved home directory under wine: {}", p.string ());
          h = move (p);
        }
      }
    #endif
    }

    if (!h.empty ())
    {
      candidates.push_back (h / ".steam" / "steam");
      candidates.push_back (h / ".local" / "share" / "Steam");

      // Check for the Flatpak installation.
      //
      // This is located in the user's .var directory.
      //
      candidates.push_back (
        h / ".var" / "app" / "com.valvesoftware.Steam" / "data" / "Steam");
    }

#ifdef _WIN32
    // Under Wine, use Z: prefixed paths for system directories.
    //
    candidates.push_back ("Z:\\usr\\share\\steam");
    candidates.push_back ("Z:\\usr\\local\\share\\steam");
#else
    candidates.push_back ("/usr/share/steam");
    candidates.push_back ("/usr/local/share/steam");
#endif

    // Also check XDG_DATA_HOME if it is set.
    //
    if (const char* xdg = getenv ("XDG_DATA_HOME"))
    {
      log::trace_l3 (categories::steam{}, "env XDG_DATA_HOME: {}", xdg);
      candidates.push_back (fs::path (xdg) / "Steam");
    }

    log::trace_l2 (categories::steam{}, "evaluating {} linux steam path candidates", candidates.size ());

    for (const auto& p : candidates)
    {
      log::trace_l3 (categories::steam{}, "probing linux candidate: {}", p.string ());

      // We are looking for a directory that looks like a Steam root. The
      // presence of the 'steamapps' subdirectory is a good indicator.
      //
      if (fs::exists (p) && fs::is_directory (p))
      {
        fs::path steamapps (p / "steamapps");
        log::trace_l3 (categories::steam{}, "candidate exists, checking for steamapps at: {}", steamapps.string ());

        if (fs::exists (steamapps) && fs::is_directory (steamapps))
        {
          log::debug (categories::steam{}, "confirmed valid linux steam root: {}", p.string ());
          co_return p;
        }
        else
        {
          log::trace_l3 (categories::steam{}, "steamapps directory missing or invalid for candidate");
        }
      }
      else
      {
        log::trace_l3 (categories::steam{}, "candidate directory does not exist or is not a directory");
      }
    }

    log::warning (categories::steam{}, "exhausted all linux candidates without finding steam");
    co_return nullopt;
  }

  // Windows detection logic.
  //
  // On Windows, the registry is the most reliable source of truth. If that
  // fails (e.g., portable installations), we fall back to checking common
  // Program Files directories.
  //
  asio::awaitable<optional<fs::path>> steam_library_manager::
  detect_steam_path_windows ()
  {
#ifdef _WIN32
    log::trace_l2 (categories::steam{}, "probing windows registry for steam installation");

    // Try the registry first.
    //
    HKEY key;
    LONG reg_res (RegOpenKeyExA (HKEY_CURRENT_USER,
                                 "Software\\Valve\\Steam",
                                 0,
                                 KEY_READ,
                                 &key));

    if (reg_res == ERROR_SUCCESS)
    {
      char buf[MAX_PATH];
      DWORD size (sizeof (buf));

      LONG val_res (RegQueryValueExA (key,
                                      "SteamPath",
                                      nullptr,
                                      nullptr,
                                      reinterpret_cast<LPBYTE> (buf),
                                      &size));

      if (val_res == ERROR_SUCCESS)
      {
        RegCloseKey (key);
        fs::path p (buf);

        log::trace_l2 (categories::steam{}, "registry provided steam path: {}", p.string ());

        if (fs::exists (p) && validate_library_path (p))
        {
          log::debug (categories::steam{}, "registry steam path is valid");
          co_return p;
        }
        else
        {
          log::warning (categories::steam{}, "registry steam path exists but failed validation: {}", p.string ());
        }
      }
      else
      {
        log::warning (categories::steam{}, "failed to read SteamPath from registry, error code: {}", val_res);
        RegCloseKey (key);
      }
    }
    else
    {
      log::trace_l2 (categories::steam{}, "failed to open registry key Software\\Valve\\Steam, error code: {}", reg_res);
    }

    log::trace_l2 (categories::steam{}, "registry lookup failed or invalid, falling back to standard windows paths");

    // If the registry lookup failed, try the standard installation paths.
    //
    vector<fs::path> candidates =
    {
      "C:\\Program Files (x86)\\Steam",
      "C:\\Program Files\\Steam"
    };

    for (const auto& p : candidates)
    {
      log::trace_l3 (categories::steam{}, "probing windows fallback candidate: {}", p.string ());

      if (fs::exists (p) && validate_library_path (p))
      {
        log::debug (categories::steam{}, "found valid windows steam root from fallback: {}", p.string ());
        co_return p;
      }
      else
      {
        log::trace_l3 (categories::steam{}, "windows fallback candidate invalid or missing");
      }
    }
#endif

    log::warning (categories::steam{}, "exhausted all windows steam candidates");
    co_return nullopt;
  }

  // macOS detection logic.
  //
  // On macOS, Steam usually lives in the user's Library/Application Support.
  //
  asio::awaitable<optional<fs::path>> steam_library_manager::
  detect_steam_path_macos ()
  {
    vector<fs::path> candidates;

    const char* home (getenv ("HOME"));
    log::trace_l3 (categories::steam{}, "env HOME: {}", home ? home : "<null>");

    if (home)
    {
      candidates.push_back (fs::path (home) / "Library" /
                            "Application Support" / "Steam");
    }

    candidates.push_back ("/Applications/Steam.app/Contents/MacOS");

    log::trace_l2 (categories::steam{}, "evaluating {} macos steam path candidates", candidates.size ());

    for (const auto& p : candidates)
    {
      log::trace_l3 (categories::steam{}, "probing macos candidate: {}", p.string ());

      if (fs::exists (p) && validate_library_path (p))
      {
        log::debug (categories::steam{}, "found valid macos steam root: {}", p.string ());
        co_return p;
      }
      else
      {
        log::trace_l3 (categories::steam{}, "macos candidate invalid or missing");
      }
    }

    log::warning (categories::steam{}, "exhausted all macos steam candidates");
    co_return nullopt;
  }

  asio::awaitable<steam_config_paths> steam_library_manager::
  get_config_paths ()
  {
    log::trace_l2 (categories::steam{}, "resolving steam configuration paths");

    if (!steam_path_)
    {
      log::trace_l3 (categories::steam{}, "steam path not cached, forcing detection");
      co_await detect_steam_path ();
    }

    steam_config_paths paths;

    if (steam_path_)
    {
      paths.steam_root = *steam_path_;
      paths.steamapps = *steam_path_ / "steamapps";
      paths.libraryfolders_vdf = paths.steamapps / "libraryfolders.vdf";
      paths.config_vdf = *steam_path_ / "config" / "config.vdf";

      log::trace_l2 (categories::steam{}, "resolved libraryfolders.vdf: {}", paths.libraryfolders_vdf.string ());
      log::trace_l2 (categories::steam{}, "resolved config.vdf: {}", paths.config_vdf.string ());
    }
    else
    {
      log::warning (categories::steam{}, "cannot resolve config paths, steam_path is null");
    }

    co_return paths;
  }

  asio::awaitable<vector<steam_library>> steam_library_manager::
  load_libraries ()
  {
    log::trace_l1 (categories::steam{}, "loading steam libraries");

    // If we have already parsed the libraries, return the cached result.
    //
    if (libraries_loaded_)
    {
      log::trace_l2 (categories::steam{}, "returning {} cached steam libraries", libraries_.size ());
      co_return libraries_;
    }

    auto paths (co_await get_config_paths ());

    if (paths.libraryfolders_vdf.empty () || !fs::exists (paths.libraryfolders_vdf))
    {
      log::error (categories::steam{}, "libraryfolders.vdf missing or unresolvable at: {}",
                            paths.libraryfolders_vdf.empty () ? "<empty>" : paths.libraryfolders_vdf.string ());
      co_return vector<steam_library> ();
    }

    log::trace_l2 (categories::steam{}, "parsing library folders from {}", paths.libraryfolders_vdf.string ());

    libraries_ = co_await parse_library_folders (ioc_, paths.libraryfolders_vdf);
    libraries_loaded_ = true;

    log::info (categories::steam{}, "loaded {} steam libraries", libraries_.size ());
    for (size_t i (0); i < libraries_.size (); ++i)
    {
      log::trace_l3 (categories::steam{}, "library [{}]: {}", i, libraries_[i].path.string ());
    }

    co_return libraries_;
  }

  // Try to find an installation of Modern Warfare 2 (IW4).
  //
  // Querying Steam app manifest is the most reliable way to find installed
  // games. The manifest contains the exact installation directory name. If
  // the manifest lookup fails, we fall back to scanning library folders by
  // common directory names.
  //
  asio::awaitable<optional<fs::path>> steam_library_manager::
  find_app (uint32_t appid)
  {
    log::info (categories::steam{}, "attempting to locate installation for appid {}", appid);

    // First, try the manifest approach which is more reliable.
    //
    // The manifest file (appmanifest_<appid>.acf) contains the "installdir"
    // field with the exact folder name under steamapps/common/.
    //
    auto manifest (co_await load_app_manifest (appid));

    if (manifest)
    {
      log::trace_l2 (categories::steam{}, "manifest loaded, checking installdir: {}", manifest->installdir);

      if (!manifest->fullpath.empty ())
      {
        fs::path p (manifest->fullpath.lexically_normal ().make_preferred ());
        log::trace_l3 (categories::steam{}, "normalized manifest path: {}", p.string ());

        if (fs::exists (p) && fs::is_directory (p))
        {
          log::info (categories::steam{}, "found appid {} via manifest at {}", appid, p.string ());
          co_return p;
        }
        else
        {
          log::warning (categories::steam{}, "manifest path does not exist or is not a directory: {}", p.string ());
        }
      }
      else
      {
        log::warning (categories::steam{}, "manifest parsed but fullpath was empty");
      }
    }

    log::debug (categories::steam{}, "manifest lookup failed for appid {}, falling back to library scan", appid);

    // Fallback: scan library folders by known directory names.
    //
    auto libs (co_await load_libraries ());

    // The common directory names used by MW2.
    //
    const char* names[] = {"Call of Duty Modern Warfare 2"};

    log::trace_l2 (categories::steam{}, "scanning {} libraries for fallback directory names", libs.size ());

    // Scan all library folders.
    //
    for (const auto& lib : libs)
    {
      log::trace_l3 (categories::steam{}, "scanning library: {}", lib.path.string ());

      for (const char* name : names)
      {
        fs::path p ((lib.path / "steamapps" / "common" / name)
                      .lexically_normal ()
                      .make_preferred ());

        log::trace_l3 (categories::steam{}, "probing fallback path: {}", p.string ());

        if (fs::exists (p) && fs::is_directory (p))
        {
          log::info (categories::steam{}, "found appid {} via fallback scan at {}", appid, p.string ());
          co_return p;
        }
      }
    }

    log::warning (categories::steam{}, "could not locate installation for appid {}", appid);
    co_return nullopt;
  }

  asio::awaitable<optional<steam_app_manifest>> steam_library_manager::
  load_app_manifest (uint32_t appid)
  {
    log::trace_l2 (categories::steam{}, "loading manifest for appid {}", appid);
    auto libs (co_await load_libraries ());

    for (const auto& lib : libs)
    {
      log::trace_l3 (categories::steam{}, "checking library {} for appid {} manifest", lib.path.string (), appid);

      if (auto mp = find_app_manifest_file (lib, appid))
      {
        log::debug (categories::steam{}, "found manifest file at {}", mp->string ());

        try
        {
          auto m (co_await parse_app_manifest (ioc_, *mp));
          log::trace_l2 (categories::steam{}, "parsed manifest for appid {}, installdir: {}", appid, m.installdir);

          // Resolve the full installation path.
          //
          if (!m.installdir.empty ())
          {
            m.fullpath = (lib.path / "steamapps" / "common" / m.installdir)
                           .lexically_normal ()
                           .make_preferred ();
            log::trace_l3 (categories::steam{}, "resolved full installation path: {}", m.fullpath.string ());
          }
          else
          {
            log::warning (categories::steam{}, "manifest parsed but installdir was empty");
          }

          co_return m;
        }
        catch (const std::exception& e)
        {
          log::error (categories::steam{}, "exception while parsing app manifest for appid {}: {}", appid, e.what ());
          // If we fail to parse a manifest, we assume it's corrupt or locked
          // and simply move on to the next library.
        }
        catch (...)
        {
          log::error (categories::steam{}, "unknown exception while parsing app manifest for appid {}", appid);
        }
      }
      else
      {
        log::trace_l3 (categories::steam{}, "manifest file not found in this library");
      }
    }

    log::trace_l2 (categories::steam{}, "manifest for appid {} not found in any library", appid);
    co_return nullopt;
  }

  asio::awaitable<map<uint32_t, fs::path>> steam_library_manager::
  get_all_apps ()
  {
    log::trace_l1 (categories::steam{}, "gathering all installed steam apps across all libraries");

    map<uint32_t, fs::path> result;
    auto libs (co_await load_libraries ());

    for (const auto& lib : libs)
    {
      fs::path apps_dir ((lib.path / "steamapps")
                           .lexically_normal ()
                           .make_preferred ());

      log::trace_l2 (categories::steam{}, "scanning steamapps directory: {}", apps_dir.string ());

      if (!fs::exists (apps_dir) || !fs::is_directory (apps_dir))
      {
        log::warning (categories::steam{}, "steamapps directory missing or invalid: {}", apps_dir.string ());
        continue;
      }

      // Scan the directory for manifest files.
      //
      // We iterate over the directory entries looking for files matching the
      // appmanifest_*.acf pattern.
      //
      for (const auto& entry : fs::directory_iterator (apps_dir))
      {
        if (!entry.is_regular_file ())
          continue;

        string name (entry.path ().filename ().string ());

        if (name.starts_with ("appmanifest_") && name.ends_with (".acf"))
        {
          log::trace_l3 (categories::steam{}, "found potential app manifest: {}", name);

          try
          {
            // Extract the AppID from the filename.
            //
            // We strip the prefix (12 chars) and the extension (4 chars).
            //
            string id_str (name.substr (12));
            id_str = id_str.substr (0, id_str.size () - 4);

            uint32_t id (stoul (id_str));
            log::trace_l3 (categories::steam{}, "extracted appid {} from {}", id, name);

            // Parse the manifest to get the installation directory.
            //
            auto m (co_await parse_app_manifest (ioc_, entry.path ()));

            if (!m.installdir.empty ())
            {
              fs::path p ((lib.path / "steamapps" / "common" / m.installdir)
                            .lexically_normal ()
                            .make_preferred ());

              if (fs::exists (p))
              {
                log::trace_l3 (categories::steam{}, "mapped appid {} to {}", id, p.string ());
                result[id] = p;
              }
              else
              {
                log::trace_l3 (categories::steam{}, "installdir mapped for appid {} but path does not exist: {}", id, p.string ());
              }
            }
            else
            {
              log::trace_l3 (categories::steam{}, "manifest for appid {} had no installdir", id);
            }
          }
          catch (const std::exception& e)
          {
            log::warning (categories::steam{}, "failed to process manifest {}: {}", name, e.what ());
          }
          catch (...)
          {
            log::warning (categories::steam{}, "unknown error processing manifest: {}", name);
            // Ignore invalid or unparseable manifests.
          }
        }
      }
    }

    log::info (categories::steam{}, "found {} installed steam apps in total", result.size ());
    co_return result;
  }

  bool steam_library_manager::
  validate_library_path (const fs::path& p)
  {
    fs::path np (p.lexically_normal ().make_preferred ());
    log::trace_l3 (categories::steam{}, "validating normalized library path: {}", np.string ());

    if (!fs::exists (np) || !fs::is_directory (np))
    {
      log::trace_l3 (categories::steam{}, "library path does not exist or is not a directory");
      return false;
    }

    // A valid Steam library must contain a 'steamapps' subdirectory.
    //
    fs::path steamapps (np / "steamapps");
    if (!fs::exists (steamapps) || !fs::is_directory (steamapps))
    {
      log::trace_l3 (categories::steam{}, "library path lacks a valid steamapps subdirectory");
      return false;
    }

    log::trace_l3 (categories::steam{}, "library path validated");
    return true;
  }

  optional<fs::path> steam_library_manager::
  find_app_manifest_file (const steam_library& lib, uint32_t appid)
  {
    fs::path apps_dir (
      (lib.path / "steamapps").lexically_normal ().make_preferred ());

    if (!fs::exists (apps_dir) || !fs::is_directory (apps_dir))
    {
      log::trace_l3 (categories::steam{}, "steamapps directory invalid during manifest search: {}", apps_dir.string ());
      return nullopt;
    }

    // Construct the expected manifest filename.
    //
    string name ("appmanifest_" + std::to_string (appid) + ".acf");
    fs::path p (apps_dir / name);

    log::trace_l3 (categories::steam{}, "checking for app manifest file at {}", p.string ());
    if (fs::exists (p) && fs::is_regular_file (p))
    {
      log::trace_l3 (categories::steam{}, "manifest file exists");
      return p;
    }

    log::trace_l3 (categories::steam{}, "manifest file does not exist");
    return nullopt;
  }
}
