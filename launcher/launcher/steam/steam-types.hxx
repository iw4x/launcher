#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <optional>
#include <filesystem>

namespace launcher
{
  namespace fs = std::filesystem;

  // VDF value types.
  //
  enum class vdf_value_type
  {
    string,
    object
  };

  // Steam library folder information.
  //
  struct steam_library
  {
    std::string label;                       // Library label/name
    fs::path path;                           // Absolute path to library folder
    std::uint64_t contentid;                 // Content ID
    std::uint64_t totalsize;                 // Total size in bytes
    std::map<std::string, std::string> apps; // App ID -> install path mappings

    steam_library () = default;

    steam_library (std::string l, fs::path p)
        : label (std::move (l)), path (std::move (p)),
          contentid (0), totalsize (0) {}
  };

  // Steam app manifest information (appmanifest_*.acf).
  //
  struct steam_app_manifest
  {
    std::uint32_t appid;                         // Application ID
    std::string name;                            // Application name
    std::string installdir;                      // Installation directory name
    fs::path fullpath;                           // Full installation path
    std::uint64_t size_on_disk;                  // Size on disk in bytes
    std::uint32_t buildid;                       // Build ID
    std::string last_updated;                    // Last update timestamp
    std::map<std::string, std::string> metadata; // Additional metadata

    steam_app_manifest ()
        : appid (0), size_on_disk (0), buildid (0) {}
  };

  // Steam configuration paths for different platforms.
  //
  struct steam_config_paths
  {
    fs::path steam_root;         // Main Steam installation directory
    fs::path config_vdf;         // config.vdf location
    fs::path libraryfolders_vdf; // libraryfolders.vdf location
    fs::path steamapps;          // steamapps directory

    steam_config_paths () = default;
  };

  // Steam app IDs for MW2.
  //
  namespace steam_appid
  {
    constexpr std::uint32_t mw2_multiplayer  = 10190;
    constexpr std::uint32_t mw2_singleplayer = 10180;
  }

  // Error codes for Steam operations.
  //
  enum class steam_error
  {
    none,
    steam_not_found,
    config_not_found,
    library_not_found,
    app_not_found,
    parse_error,
    invalid_path,
    permission_denied
  };

  std::string
  to_string (steam_error);

  std::string
  to_string (vdf_value_type);
}
