// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>

#include <launcher/arch/arch.hxx>
#include <launcher/shortcut/shortcut.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // Shortcuts that start each client through the launcher.
  //
  class shortcut_coordinator
  {
  public:
    // The shortcuts start the launcher executable in the installation root
    // directory.
    //
    shortcut_coordinator (fs::path launcher, fs::path root);

    shortcut_coordinator (const shortcut_coordinator&) = delete;
    shortcut_coordinator& operator= (const shortcut_coordinator&) = delete;

    // Bring the shortcuts of every client up to date, including their
    // Steam library entries and the artwork that they show. We log failures
    // and otherwise ignore them.
    //
    void
    refresh () noexcept;

    // Return the shortcut specification of the architecture's client.
    //
    shortcut_spec
    spec (architecture) const;

  private:
    fs::path launcher_;
    fs::path root_;
  };
}
