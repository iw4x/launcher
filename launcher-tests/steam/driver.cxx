// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <print>
#include <string>
#include <vector>
#include <cstdlib>      // exit(), getenv()
#include <fstream>
#include <iostream>
#include <algorithm>    // ranges::sort()
#include <iterator>
#include <exception>
#include <filesystem>
#include <string_view>

#include <launcher/shortcut/shortcut-vdf.hxx>
#include <launcher/shortcut/shortcut-steam.hxx>
#include <launcher/shortcut/shortcut-types.hxx>

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

  // Read the shortcuts from stdin (see main() for the form).
  //
  vector<shortcut_spec>
  read_specs ()
  {
    vector<shortcut_spec> r;
    bool next (true);

    for (string l; getline (cin, l); )
    {
      if (l.empty ())
      {
        next = true;
        continue;
      }

      if (next)
      {
        r.emplace_back ();
        next = false;
      }

      shortcut_spec& s (r.back ());

      size_t p (l.find (' '));
      string k (l.substr (0, p));
      string v (p != string::npos ? l.substr (p + 1) : string ());

      if      (k == "name")      s.name = v;
      else if (k == "target")    s.target = v;
      else if (k == "directory") s.working_directory = v;
      else if (k == "argument")  s.arguments.push_back (v);
      else if (k == "legacy")    s.legacy_names.push_back (v);
      else if (k == "cover")     s.library.cover = filesystem::absolute (v);
      else
        fail ("invalid shortcut line '" + l + "'");
    }

    return r;
  }

  // Print the entries of each signed-in account's shortcuts list.
  //
  void
  print_lists (const filesystem::path& home)
  {
    filesystem::path r (home / ".steam/steam");

    if (!filesystem::exists (r))
      return;

    vector<filesystem::path> ps (
      steam_shortcuts::lists (filesystem::canonical (r)));

    ranges::sort (ps);

    for (const filesystem::path& p: ps)
    {
      println ("{}", filesystem::relative (p, home).generic_string ());

      ifstream is (p, ios::binary);
      if (!is)
        continue;

      vector<char> d ((istreambuf_iterator<char> (is)),
                      istreambuf_iterator<char> ());

      vdf_object o (vdf_object::parse (d));
      const vdf_object* l (o.object ("shortcuts"));

      if (l == nullptr)
        continue;

      for (const string& k: l->keys ())
      {
        const vdf_object& e (*l->object (k));

        println ("  {}", k);
        println ("    appid {}", *e.number ("appid"));

        for (const char* f: {"AppName", "Exe", "StartDir", "LaunchOptions"})
          println ("    {} {}", f, *e.string (f));
      }
    }
  }
}

// Usage: argv[0] <command> [<argument>...]
//
// app-id <exe> <name>
//    Print the app id that Steam assigns to the non-Steam game shortcut.
//
// lists
//    Print the shortcuts lists of the Steam installation in .steam/steam/
//    of the home directory, relative to it, sorted.
//
// write
//    Read the shortcuts from stdin, write them to the Steam installation in
//    the home directory, and print the outcome followed by the lists with
//    their entries. Blank lines separate the shortcuts and each consists of
//    the following lines, with the last three repeatable:
//
//    name <name>
//    target <path>
//    directory <path>
//    cover <path>
//    argument <argument>
//    legacy <name>
//
// Note that lists and write insist on the home directory being the current
// working directory since write modifies the Steam installation it finds
// there and we never want it to reach a real one.
//
int
main (int argc, char* argv[])
{
  vector<string> as (argv + 1, argv + argc);
  string_view c (!as.empty () ? string_view (as[0]) : string_view ());

  try
  {
    if (c == "app-id" && as.size () == 3)
    {
      println ("{}", steam_shortcuts::app_id (as[1], as[2]));
      return 0;
    }

    const char* h (getenv ("HOME"));

    if (h == nullptr ||
        !filesystem::equivalent (h, filesystem::current_path ()))
      fail ("HOME is not the current working directory");

    if (c == "lists" && as.size () == 1)
    {
      vector<filesystem::path> ps (
        steam_shortcuts::lists (filesystem::canonical (".steam/steam")));

      ranges::sort (ps);

      for (const filesystem::path& p: ps)
        println ("{}", filesystem::relative (p).generic_string ());
    }
    else if (c == "write" && as.size () == 1)
    {
      println ("{}", to_string (steam_shortcuts::write (read_specs ())));
      print_lists (h);
    }
    else
      fail ("invalid arguments\n"
            "  info: usage: driver <command> [<argument>...]");
  }
  catch (const exception& e)
  {
    fail (e.what ());
  }
}
