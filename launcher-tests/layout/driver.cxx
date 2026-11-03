// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <vector>
#include <cstdlib>      // exit()
#include <iostream>
#include <iterator>
#include <exception>
#include <filesystem>
#include <string_view>

#include <launcher/launcher-manifest.hxx>

using namespace std;
using namespace launcher;

namespace
{
  [[noreturn]] void
  fail (const string& m)
  {
    cerr << "error: " << m << endl;
    exit (1);
  }

  string
  read_input ()
  {
    return string ((istreambuf_iterator<char> (cin)),
                   istreambuf_iterator<char> ());
  }

  void
  print_file (const manifest_file& f, string_view indent)
  {
    cout << indent << "file " << f.path
         << ' ' << f.size
         << ' ' << (f.hash.empty () ? "-" : f.hash.value);

    if (f.asset_name)
      cout << " asset=" << *f.asset_name;

    if (f.archive_name)
      cout << " archive=" << *f.archive_name;

    cout << endl;
  }
}

// Usage: argv[0] <command>
//
// parse
//    Read an update manifest from stdin and print its archives, each
//    followed by the files linked to it, and then all its files:
//
//    archive <name> <size> <url> <hash>
//      file <path> <size> <hash> [asset=<name>] [archive=<name>]
//    file <path> <size> <hash> [asset=<name>] [archive=<name>]
//
//    An absent value prints as '-'.
//
// parse-dlc
//    As parse but read a DLC manifest.
//
// serialize
//    Read an update manifest from stdin and print it serialized back to
//    JSON.
//
// resolve
//    Read manifest entries from stdin, one per line, in the following forms
//    and print where each is installed, relative to the installation
//    directory:
//
//    file <path>
//    archive <name>
//
int
main (int argc, char* argv[])
{
  string_view c (argc == 2 ? argv[1] : "");

  try
  {
    if (c == "parse" || c == "parse-dlc")
    {
      manifest m (manifest_coordinator::parse (read_input (),
                                               c == "parse"
                                               ? manifest_format::update
                                               : manifest_format::dlc));

      for (const manifest_archive& a: m.archives)
      {
        cout << "archive " << a.name
             << ' ' << a.size
             << ' ' << (a.url.empty () ? "-" : a.url)
             << ' ' << (a.hash.empty () ? "-" : a.hash.value) << endl;

        for (const manifest_file& f: a.files)
          print_file (f, "  ");
      }

      for (const manifest_file& f: m.files)
        print_file (f, "");
    }
    else if (c == "serialize")
    {
      cout << manifest_coordinator::parse (read_input ()).string () << endl;
    }
    else if (c == "resolve")
    {
      for (string l; getline (cin, l); )
      {
        size_t p (l.find (' '));
        string k (l.substr (0, p));
        string v (p != string::npos ? l.substr (p + 1) : string ());

        filesystem::path r;

        if (k == "file")
        {
          manifest_file f;
          f.path = v;
          r = manifest_coordinator::resolve_path (f, "root");
        }
        else if (k == "archive")
        {
          manifest_archive a;
          a.name = v;
          r = manifest_coordinator::resolve_path (a, "root");
        }
        else
          fail ("invalid entry '" + l + "'");

        cout << r.lexically_relative ("root").generic_string () << endl;
      }
    }
    else
      fail ("invalid arguments\n"
            "  info: usage: driver <command>");
  }
  catch (const exception& e)
  {
    fail (e.what ());
  }
}
