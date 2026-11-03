// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/installation.hxx>

#include <chrono>
#include <utility>      // move()
#include <functional>   // hash
#include <system_error>

#include <launcher/utility.hxx>
#include <launcher/launcher-log.hxx>
#include <launcher/arch/arch-types.hxx>
#include <launcher/cache/cache-database.hxx>

using namespace std;
using namespace std::filesystem;

namespace launcher
{
  namespace
  {
    // Rename the client that earlier launchers installed under the legacy
    // name to the current name of the architecture's client, unless the
    // latter already exists.
    //
    void
    migrate_client (const path& root, const char* legacy, architecture a)
    {
      path f (root / from_utf8 (legacy));
      path t (root / from_utf8 (architecture_executable (a)));

      error_code ec;

      if (!exists (f, ec) || exists (t, ec))
        return;

      rename (f, t, ec);

      if (ec)
        log::warning (categories::launcher (),
                      "failed to rename {} to {}: {}",
                      to_utf8 (f),
                      to_utf8 (t),
                      to_utf8_system_message (ec.message ()));
      else
        log::info (categories::launcher (),
                   "renamed {} to {}",
                   to_utf8 (f),
                   to_utf8 (t));
    }
  }

  installation::
  installation (path root)
    : root_ (move (root))
  {
  }

  path installation::
  cache () const
  {
    return root_ / cache_database::dir_name;
  }

  path installation::
  retired () const
  {
    return cache () / "retired";
  }

  void installation::
  migrate_clients () const
  {
    migrate_client (root_, "iw4x (x86).exe", architecture::x86);
    migrate_client (root_, "iw4x (x64).exe", architecture::x64);
  }

  void installation::
  migrate_dlc (const vector<path>& fs) const
  {
    path d (root_ / "zone" / "iw4x" / "x86" / "dlc");
    path l (root_ / "zone" / "dlc");

    for (const path& t : fs)
    {
      if (t.parent_path () != d)
        continue;

      path f (l / t.filename ());

      // Note that the legacy copy can be a symlink, which may dangle.
      //
      error_code ec;

      if (!exists (symlink_status (f, ec)))
        continue;

      if (exists (t, ec))
      {
        remove (f, ec);

        if (ec)
          throw system_error (
            ec,
            "unable to remove " + to_utf8 (f) + ", which the client would "
            "move over " + to_utf8 (t) + " at startup; remove it or make " +
            to_utf8 (l) + " writable");

        log::info (categories::launcher (),
                   "removed {} which {} supersedes",
                   to_utf8 (f),
                   to_utf8 (t));

        continue;
      }

      create_directories (d, ec);

      if (!ec)
        rename (f, t, ec);

      if (ec)
        throw system_error (
          ec,
          "unable to move " + to_utf8 (f) + " to " + to_utf8 (t) +
          ", which the client would do at startup; move it or make " +
          to_utf8 (l) + " writable");

      log::info (categories::launcher (),
                 "moved {} to {}",
                 to_utf8 (f),
                 to_utf8 (t));
    }
  }

  bool installation::
  retire (const path& f) const
  {
    error_code ec;

    path d (retired ());
    create_directories (d, ec);

    if (ec)
    {
      log::warning (categories::launcher (),
                    "failed to create retired directory {}: {}",
                    to_utf8 (d),
                    to_utf8_system_message (ec.message ()));
      return false;
    }

    // An earlier copy of the same file may still be retired and in use, so
    // make the name unique.
    //
    path t (d / (path_digest (f) + '_' +
                 std::to_string (
                   chrono::system_clock::now ().time_since_epoch ().count ()) +
                 '_' + to_utf8 (f.filename ())));

    rename (f, t, ec);

    if (ec)
    {
      log::warning (categories::launcher (),
                    "failed to retire {}: {}",
                    to_utf8 (f),
                    to_utf8_system_message (ec.message ()));
      return false;
    }

    log::info (categories::launcher (),
               "retired {} which is in use by another process",
               to_utf8 (f));
    return true;
  }

  void installation::
  remove_retired () const
  {
    path d (retired ());
    error_code ec;

    if (!exists (d, ec))
      return;

    for (directory_iterator i (d, ec), e; !ec && i != e; i.increment (ec))
    {
      error_code rc;
      remove (i->path (), rc);

      if (rc)
        log::trace_l2 (categories::launcher (),
                       "retired file {} is still in use: {}",
                       to_utf8 (i->path ()),
                       to_utf8_system_message (rc.message ()));
      else
        log::trace_l2 (categories::launcher (),
                       "removed retired file {}",
                       to_utf8 (i->path ()));
    }
  }

  bool installation::
  reset_cache (const string& version) const
  {
    const string key ("launcher_version");

    // See which version the cache belongs to.
    //
    string v;
    {
      cache_database db (root_);
      v = db.setting_value (key);
    }

    if (v == version)
      return false;

    // Wipe the cache directory, recreating it afterwards since it also holds
    // the database.
    //
    path d (cache ());
    error_code ec;

    if (exists (d, ec))
    {
      log::info (categories::launcher (),
                 "launcher version changed ({} -> {}), wiping local cache "
                 "directory: {}",
                 v.empty () ? "none" : v,
                 version,
                 to_utf8 (d));

      remove_all (d, ec);

      if (ec)
        log::warning (categories::launcher (),
                      "failed to remove cache directory: {}",
                      to_utf8_system_message (ec.message ()));

      create_directories (d, ec);
    }

    // Record the version in the database, which may be new.
    //
    cache_database db (root_);
    db.setting (key, version);

    return true;
  }

  string
  path_digest (const path& p)
  {
    return std::to_string (hash<string> () (to_utf8 (p)));
  }
}
