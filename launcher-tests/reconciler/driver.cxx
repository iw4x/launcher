// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <print>
#include <chrono>
#include <string>
#include <vector>
#include <cstdlib>      // exit()
#include <sstream>
#include <utility>      // pair
#include <iostream>
#include <optional>
#include <algorithm>    // ranges::find(), ranges::sort()
#include <exception>
#include <filesystem>

#include <launcher/launcher-manifest.hxx>
#include <launcher/cache/cache-types.hxx>
#include <launcher/cache/cache-database.hxx>
#include <launcher/cache/cache-reconciler.hxx>

using namespace std;
using namespace launcher;

namespace
{
  [[noreturn]] void
  fail (const string& m)
  {
    println (cerr, "error: {}", m);
    exit (1);
  }

  const pair<const char*, component_type> components[] =
  {
    {"client",     component_type::client},
    {"rawfiles",   component_type::rawfiles},
    {"dlc",        component_type::dlc},
    {"helper",     component_type::helper},
    {"launcher",   component_type::launcher},
    {"client-x64", component_type::client_x64},
    {"assets-x64", component_type::assets_x64}
  };

  component_type
  to_component (const string& s)
  {
    auto i (ranges::find (components, s, [] (const auto& c)
    {
      return string (c.first);
    }));

    if (i == ranges::end (components))
      fail ("invalid component '" + s + "'");

    return i->second;
  }

  strategy
  to_strategy (const string& s)
  {
    if (s == "mtime") return strategy::mtime;
    if (s == "mixed") return strategy::mixed;
    if (s == "hash")  return strategy::hash;

    fail ("invalid strategy '" + s + "'");
  }

  template <typename T>
  string
  to_text (const T& v)
  {
    ostringstream o;
    o << v;
    return o.str ();
  }
}

// Usage: argv[0] <command> [<argument>...]
//
// Run the command with the reconciler of the installation in the current
// working directory and print its result to stdout, with the paths relative
// to the installation.
//
// plan <manifest> <component> <version> [<strategy>]
//    Plan the update of the component to the version of the manifest file
//    and print the items followed by the summary:
//
//    download <path> <size> <hash>
//    summary <downloads> <bytes>
//
// track <component> <version> <path>...
//    Record the files as installed for the component version.
//
// files <component>
//    Print the recorded files of the component, sorted by path:
//
//    <path> <version> <size> <hash>
//
// stat <path> [<strategy>]
//    Print the state of the file.
//
// clean <manifest> <component>
//    Forget the recorded files of the component that the manifest file no
//    longer lists and print them, sorted.
//
// stamp <component> <tag>
//    Record the component as installed at the tag.
//
// outdated <component> <tag>
//    Print whether the component is outdated compared to the tag.
//
// set-mtime <path>
//    Set the modification time of the file to 2000-01-01.
//
// The strategy names the reconciler strategy, mtime by default. An absent
// hash prints as '-'.
//
int
main (int argc, char* argv[])
{
  vector<string> as (argv + 1, argv + argc);
  const string c (!as.empty () ? as[0] : "");
  const size_t n (as.size ());

  try
  {
    const filesystem::path root (filesystem::current_path ());

    if (c == "set-mtime" && n == 2)
    {
      // Pick a time that every platform can represent. Wine, for example,
      // stops at 2106.
      //
      using namespace chrono;

      filesystem::last_write_time (
        as[1],
        clock_cast<file_clock> (sys_days (year (2000) / January / 1)));
      return 0;
    }

    cache_database db (root);
    reconciler r (db, root);

    auto rel = [&r] (const filesystem::path& p)
    {
      return p.lexically_relative (r.root ()).generic_string ();
    };

    auto hash = [] (const string& h) {return h.empty () ? "-" : h;};

    if (c == "plan" && (n == 4 || n == 5))
    {
      if (n == 5)
        r.mode (to_strategy (as[4]));

      manifest m (manifest_coordinator::load (as[1]));
      vector<reconcile_item> is (r.plan (m, to_component (as[2]), as[3]));

      for (const reconcile_item& i: is)
        println ("{} {} {} {}",
                 to_text (i.action),
                 rel (i.path),
                 i.expected_size,
                 hash (i.expected_hash));

      reconcile_summary s (r.summarize (is));
      println ("summary {} {}", s.downloads_required, s.bytes_to_download);
    }
    else if (c == "track" && n >= 4)
    {
      vector<filesystem::path> ps;

      for (size_t i (3); i != n; ++i)
        ps.push_back (r.root () / as[i]);

      if (ps.size () == 1)
        r.track (ps[0], to_component (as[1]), as[2]);
      else
        r.track (ps, to_component (as[1]), as[2]);
    }
    else if (c == "files" && n == 2)
    {
      vector<cached_file> fs (db.files (to_component (as[1])));
      ranges::sort (fs, {}, &cached_file::path);

      for (const cached_file& f: fs)
        println ("{} {} {} {}",
                 rel (f.path ()),
                 f.version (),
                 f.size (),
                 hash (f.hash ()));
    }
    else if (c == "stat" && (n == 2 || n == 3))
    {
      if (n == 3)
        r.mode (to_strategy (as[2]));

      println ("{}", to_text (r.stat (r.root () / as[1])));
    }
    else if (c == "clean" && n == 3)
    {
      vector<string> os (r.clean (manifest_coordinator::load (as[1]),
                                  to_component (as[2])));
      ranges::sort (os);

      for (const string& o: os)
        println ("{}", rel (o));
    }
    else if (c == "stamp" && n == 3)
      r.stamp (to_component (as[1]), as[2]);
    else if (c == "outdated" && n == 3)
      println ("{}", r.outdated (to_component (as[1]), as[2]));
    else
      fail ("invalid arguments\n"
            "  info: usage: driver <command> [<argument>...]");
  }
  catch (const exception& e)
  {
    fail (e.what ());
  }
}
