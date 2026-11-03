// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace launcher
{
  // Installation directory with the launcher and its clients. It also holds
  // the launcher cache, in the cache/ subdirectory.
  //
  class installation
  {
  public:
    // Create the installation in the specified root directory. It should be
    // absolute and normalized.
    //
    explicit
    installation (std::filesystem::path root);

    const std::filesystem::path&
    root () const noexcept {return root_;}

    // Return the cache directory.
    //
    std::filesystem::path
    cache () const;

    // Return the directory with the files that were moved out of the way
    // since they could not be removed (see retire()).
    //
    std::filesystem::path
    retired () const;

    // Rename the clients that earlier launchers installed as 'iw4x (x86).exe'
    // and 'iw4x (x64).exe' to their current names, unless clients with the
    // current names already exist.
    //
    void
    migrate_clients () const;

    // Move the DLC fastfiles that earlier launchers installed into zone/dlc/
    // to their destinations among the specified files, which are the files
    // that the DLC manifest installs. If a destination already exists, then
    // remove the legacy copy instead.
    //
    // At startup the client moves the files of zone/dlc/ that are not the
    // game's own DLC over their copies in zone/iw4x/x86/dlc/, taking them
    // for the newer ones. So a legacy copy that differs from the DLC
    // manifest's replaces the installed file on every start that finds it,
    // and we download the file again on the next update.
    //
    void
    migrate_dlc (const std::vector<std::filesystem::path>&) const;

    // Move a file that is in use by another process, and so cannot be
    // removed, into the retired directory. Return true if the file was moved.
    //
    // Note that the retired directory is in the installation and so the
    // rename stays within the same volume, which is what makes it work for
    // a loaded module.
    //
    bool
    retire (const std::filesystem::path&) const;

    // Remove the retired files that are no longer in use.
    //
    void
    remove_retired () const;

    // Wipe the cache unless the specified launcher version created it and
    // record this version in the cache. Return true if the version changed.
    //
    bool
    reset_cache (const std::string& version) const;

  private:
    std::filesystem::path root_;
  };

  // Return a digest of the path that is suitable as part of a file name.
  //
  std::string
  path_digest (const std::filesystem::path&);
}
