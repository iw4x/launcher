// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>

#include <launcher/shortcut/shortcut-types.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // Shortcut in the desktop environment. On Windows it is a .lnk file and
  // on Linux a desktop entry in $XDG_DATA_HOME/applications/.
  //
  class shortcut_writer
  {
  public:
    // Return true if the platform has shortcuts in the scope. Linux only
    // has the menu.
    //
    static bool
    supported (shortcut_scope) noexcept;

    // Return the directory of the shortcuts in the scope if we can
    // determine it and nullopt otherwise. Note that it is always nullopt for
    // an unsupported scope.
    //
    static std::optional<fs::path>
    directory (shortcut_scope);

    // Return the path of the shortcut's file in the scope (see directory()).
    //
    static std::optional<fs::path>
    location (const shortcut_spec&, shortcut_scope);

    // Write the shortcut unless the existing file already matches it.
    // Return true if we wrote it. Throw std::runtime_error on failures.
    //
    static bool
    write (const shortcut_spec&, shortcut_scope);
  };
}
