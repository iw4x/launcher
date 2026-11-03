// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace launcher
{
  namespace fs = std::filesystem;

  // Shortcut location in the desktop environment.
  //
  enum class shortcut_scope
  {
    desktop,
    menu     // Application menu (Start menu on Windows).
  };

  std::string_view
  to_string (shortcut_scope) noexcept;

  std::ostream&
  operator<< (std::ostream&, shortcut_scope);

  // Shortcut description, installed by shortcut_writer in the desktop
  // environment and by steam_shortcuts in the Steam library.
  //
  struct shortcut_spec
  {
    std::string id;      // File name stem (<id>.desktop on Linux).
    std::string name;    // Name shown, which also identifies the .lnk file
                         // and the Steam entry.
    std::string comment; // Tooltip text.

    // Names that earlier launchers gave this shortcut. We remove the
    // shortcuts and Steam entries that still carry one of them.
    //
    std::vector<std::string> legacy_names;

    fs::path target;
    std::vector<std::string> arguments;
    fs::path working_directory;

    fs::path icon;  // Shortcut icon (an executable's icon on Windows).
    fs::path image; // Steam entry icon (PNG).

    // Images that Steam shows for the entry in its library.
    //
    struct library_artwork
    {
      fs::path cover; // Portrait capsule.
      fs::path wide;  // Landscape capsule.
      fs::path hero;  // Banner at the top of the game page.
      fs::path logo;  // Logo drawn over the hero.
    };

    library_artwork library;
  };
}
