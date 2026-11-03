// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <launcher/shortcut/shortcut-types.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // Outcome of steam_shortcuts::write().
  //
  enum class steam_status
  {
    updated,       // Wrote the changes.
    restarted,     // Wrote the changes, closing and restarting Steam.
    up_to_date,    // Nothing to change.
    not_installed, // No Steam installation found.
    no_users,      // No account has signed in to Steam yet.
    running,       // Steam would not close, so nothing was written.
    unreadable     // A list that we cannot parse, left unchanged.
  };

  std::string_view
  to_string (steam_status) noexcept;

  std::ostream&
  operator<< (std::ostream&, steam_status);

  // Non-Steam game entries in the Steam library, one list per account
  // (userdata/<account>/config/shortcuts.vdf), together with their library
  // images (userdata/<account>/config/grid/).
  //
  // Steam keeps the lists in memory while it runs and saves them when it
  // exits, which would overwrite our changes. So we close Steam before
  // writing and start it again afterwards.
  //
  class steam_shortcuts
  {
  public:
    // Return the Steam installation directory if found and nullopt
    // otherwise.
    //
    static std::optional<fs::path>
    root ();

    // Return true if Steam is running.
    //
    static bool
    running ();

    // Return the list paths of the accounts that have signed in to the
    // Steam installed in root. A list file may not exist yet.
    //
    static std::vector<fs::path>
    lists (const fs::path& root);

    // Make every account's list hold an up-to-date entry for each shortcut,
    // with its library images installed, and no entry under its legacy
    // names. Close Steam for the writing if it is running and start it
    // again afterwards, through the companion launcher if it is specified
    // and present (Linux only). Throw std::runtime_error on failures.
    //
    // Note that we first check every list, so we only close Steam if there
    // is something to write.
    //
    static steam_status
    write (const std::vector<shortcut_spec>&,
           const fs::path& companion = {});

    // Ask the Steam installed in root to exit and wait for it for up to 30
    // seconds. Return false if it is still running.
    //
    static bool
    shutdown (const fs::path& root);

    // Start the Steam installed in root detached from us, through the
    // companion launcher if it is specified and present. Return false if
    // we could not start it.
    //
    static bool
    launch (const fs::path& root, const fs::path& companion = {});

    // Return the app id that Steam assigns to a non-Steam game entry with
    // this executable (quoted, as the entry stores it) and name. Steam
    // names the entry's library images after it.
    //
    static std::int32_t
    app_id (const std::string& exe, const std::string& name);
  };
}
