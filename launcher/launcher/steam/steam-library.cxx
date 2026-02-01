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
      HMODULE ntdll (GetModuleHandleA ("ntdll.dll"));

      if (ntdll != nullptr)
      {
        auto proc = GetProcAddress (ntdll, "wine_get_version");
        r = (proc != nullptr) ? 1 : 0;
      }
      else
        r = 0;
    }

    return r == 1;
  }
#endif

  steam_library_manager::
  steam_library_manager (asio::io_context& ioc)
    : ioc_ (ioc),
      libraries_loaded_ (false)
  {
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
    optional<fs::path> r;

#ifdef _WIN32
    // If we're running under Wine, use Linux detection since Steam is likely
    // installed on the host Linux system.
    //
    if (is_wine ())
      r = co_await detect_steam_path_linux ();
    else
      r = co_await detect_steam_path_windows ();
#elif defined(__APPLE__)
    r = co_await detect_steam_path_macos ();
#else
    r = co_await detect_steam_path_linux ();
#endif

    if (r)
      steam_path_ = *r;

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
    fs::path h (home ? home : "");

    // If we came up empty, this might be Wine.
    //
    if (h.empty ())
    {
    #ifdef _WIN32
      // Try to construct the path based on the username. Prefer USER but fall
      // back to USERNAME which is standard on Windows.
      //
      const char* u (getenv ("USER"));
      if (u == nullptr)
        u = getenv ("USERNAME");

      // Z:\home\<user>.
      //
      if (u != nullptr)
      {
        fs::path p ("Z:\\home");
        p /= u;

        if (fs::exists (p))
          h = move (p);
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
      candidates.push_back (fs::path (xdg) / "Steam");

    for (const auto& p : candidates)
    {
      // We are looking for a directory that looks like a Steam root. The
      // presence of the 'steamapps' subdirectory is a good indicator.
      //
      if (fs::exists (p) && fs::is_directory (p))
      {
        fs::path steamapps (p / "steamapps");

        if (fs::exists (steamapps) && fs::is_directory (steamapps))
          co_return p;
      }
    }

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
    // Try the registry first.
    //
    HKEY key;
    if (RegOpenKeyExA (HKEY_CURRENT_USER,
                       "Software\\Valve\\Steam",
                       0,
                       KEY_READ,
                       &key) == ERROR_SUCCESS)
    {
      char buf[MAX_PATH];
      DWORD size (sizeof (buf));

      if (RegQueryValueExA (key,
                            "SteamPath",
                            nullptr,
                            nullptr,
                            reinterpret_cast<LPBYTE> (buf),
                            &size) == ERROR_SUCCESS)
      {
        RegCloseKey (key);
        fs::path p (buf);

        if (fs::exists (p) && validate_library_path (p))
          co_return p;
      }

      RegCloseKey (key);
    }

    // If the registry lookup failed, try the standard installation paths.
    //
    vector<fs::path> candidates =
    {
      "C:\\Program Files (x86)\\Steam",
      "C:\\Program Files\\Steam"
    };

    for (const auto& p : candidates)
    {
      if (fs::exists (p) && validate_library_path (p))
        co_return p;
    }
#endif

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
    if (home)
      candidates.push_back (fs::path (home) / "Library" /
                            "Application Support" / "Steam");

    candidates.push_back ("/Applications/Steam.app/Contents/MacOS");

    for (const auto& p : candidates)
    {
      if (fs::exists (p) && validate_library_path (p))
        co_return p;
    }

    co_return nullopt;
  }

  asio::awaitable<steam_config_paths> steam_library_manager::
  get_config_paths ()
  {
    if (!steam_path_)
      co_await detect_steam_path ();

    steam_config_paths paths;

    if (steam_path_)
    {
      paths.steam_root = *steam_path_;
      paths.steamapps = *steam_path_ / "steamapps";
      paths.libraryfolders_vdf = paths.steamapps / "libraryfolders.vdf";
      paths.config_vdf = *steam_path_ / "config" / "config.vdf";
    }

    co_return paths;
  }

  asio::awaitable<vector<steam_library>> steam_library_manager::
  load_libraries ()
  {
    // If we have already parsed the libraries, return the cached result.
    //
    if (libraries_loaded_)
      co_return libraries_;

    auto paths (co_await get_config_paths ());

    if (!fs::exists (paths.libraryfolders_vdf))
      co_return vector<steam_library> ();

    libraries_ = co_await parse_library_folders (ioc_, paths.libraryfolders_vdf);
    libraries_loaded_ = true;

    co_return libraries_;
  }

  // Try to find an installation of Modern Warfare 2 (IW4).
  //
  // Querying Steam app manifest is the most reliable way to find installed
  // games, but it does not always provide a valid install path. In those cases,
  // we fall back to locating the installation directory by name.
  //
  asio::awaitable<optional<fs::path>> steam_library_manager::
  find_app (uint32_t /*appid*/)
  {
    auto libs (co_await load_libraries ());

    // The common directory names used by MW2.
    //
    const char* names[] = {"Call of Duty Modern Warfare 2"};

    // Scan all library folders.
    //
    for (const auto& lib : libs)
    {
      for (const char* name : names)
      {
        fs::path p (lib.path / "steamapps" / "common" / name);

        if (fs::exists (p) && fs::is_directory (p))
          co_return p;
      }
    }

    co_return nullopt;
  }

  asio::awaitable<optional<steam_app_manifest>> steam_library_manager::
  load_app_manifest (uint32_t appid)
  {
    auto libs (co_await load_libraries ());

    for (const auto& lib : libs)
    {
      if (auto mp = find_app_manifest_file (lib, appid))
      {
        try
        {
          auto m (co_await parse_app_manifest (ioc_, *mp));

          // Resolve the full installation path.
          //
          if (!m.installdir.empty ())
          {
            m.fullpath = lib.path / "steamapps" / "common" / m.installdir;
          }

          co_return m;
        }
        catch (...)
        {
          // If we fail to parse a manifest, we assume it's corrupt or locked
          // and simply move on to the next library.
        }
      }
    }

    co_return nullopt;
  }

  asio::awaitable<map<uint32_t, fs::path>> steam_library_manager::
  get_all_apps ()
  {
    map<uint32_t, fs::path> result;
    auto libs (co_await load_libraries ());

    for (const auto& lib : libs)
    {
      fs::path apps_dir (lib.path / "steamapps");

      if (!fs::exists (apps_dir) || !fs::is_directory (apps_dir))
        continue;

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
          try
          {
            // Extract the AppID from the filename.
            //
            // We strip the prefix (12 chars) and the extension (4 chars).
            //
            string id_str (name.substr (12));
            id_str = id_str.substr (0, id_str.size () - 4);

            uint32_t id (stoul (id_str));

            // Parse the manifest to get the installation directory.
            //
            auto m (co_await parse_app_manifest (ioc_, entry.path ()));

            if (!m.installdir.empty ())
            {
              fs::path p (lib.path / "steamapps" / "common" / m.installdir);

              if (fs::exists (p))
                result[id] = p;
            }
          }
          catch (...)
          {
            // Ignore invalid or unparseable manifests.
          }
        }
      }
    }

    co_return result;
  }

  bool steam_library_manager::
  validate_library_path (const fs::path& p)
  {
    if (!fs::exists (p) || !fs::is_directory (p))
      return false;

    // A valid Steam library must contain a 'steamapps' subdirectory.
    //
    fs::path steamapps (p / "steamapps");
    if (!fs::exists (steamapps) || !fs::is_directory (steamapps))
      return false;

    return true;
  }

  optional<fs::path> steam_library_manager::
  find_app_manifest_file (const steam_library& lib, uint32_t appid)
  {
    fs::path apps_dir (lib.path / "steamapps");

    if (!fs::exists (apps_dir) || !fs::is_directory (apps_dir))
      return nullopt;

    // Construct the expected manifest filename.
    //
    string name ("appmanifest_" + std::to_string (appid) + ".acf");
    fs::path p (apps_dir / name);

    if (fs::exists (p) && fs::is_regular_file (p))
      return p;

    return nullopt;
  }
}
