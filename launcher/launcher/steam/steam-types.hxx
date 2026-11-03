// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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

  // Kind of a value in Valve's text key-values format.
  //
  // @@ Nothing uses this enumeration.
  //
  enum class vdf_value_type
  {
    string,
    object
  };

  // Steam library folder, as recorded in libraryfolders.vdf.
  //
  // @@ Nothing uses this struct.
  //
  struct steam_library
  {
    std::string label;                       // Name that the user gave it.
    fs::path path;
    std::uint64_t contentid;
    std::uint64_t totalsize;                 // In bytes.
    std::map<std::string, std::string> apps; // Installed apps by app id.

    steam_library () = default;

    steam_library (std::string l, fs::path p)
        : label (std::move (l)), path (std::move (p)),
          contentid (0), totalsize (0) {}
  };

  // Steam app manifest (steamapps/appmanifest_<appid>.acf).
  //
  // @@ Nothing uses this struct.
  //
  struct steam_app_manifest
  {
    std::uint32_t appid;
    std::string name;
    std::string installdir;                      // Under steamapps/common/.
    fs::path fullpath;                           // Completed installdir.
    std::uint64_t size_on_disk;                  // In bytes.
    std::uint32_t buildid;
    std::string last_updated;                    // Unix time, as recorded.
    std::map<std::string, std::string> metadata; // Remaining fields.

    steam_app_manifest ()
        : appid (0), size_on_disk (0), buildid (0) {}
  };

  // Locations of the Steam installation files.
  //
  // @@ Nothing uses this struct.
  //
  struct steam_config_paths
  {
    fs::path steam_root;         // Steam installation directory.
    fs::path config_vdf;
    fs::path libraryfolders_vdf;
    fs::path steamapps;

    steam_config_paths () = default;
  };

  // Steam app ids of Modern Warfare 2.
  //
  namespace steam_appid
  {
    constexpr std::uint32_t mw2_multiplayer  = 10190;
    constexpr std::uint32_t mw2_singleplayer = 10180;
  }

  // Steam operation errors.
  //
  // @@ Nothing uses this enumeration.
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
