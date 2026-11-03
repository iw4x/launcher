// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>      // exit()
#include <sstream>
#include <utility>      // pair
#include <iostream>
#include <optional>
#include <algorithm>    // ranges::find(), ranges::sort()
#include <exception>
#include <filesystem>

#include <launcher/cache/cache-types.hxx>
#include <launcher/cache/cache-database.hxx>

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

  const char*
  to_string (component_type c)
  {
    return ranges::find (components, c, &pair<const char*,
                                              component_type>::second)->first;
  }

  void
  print_file (const cached_file& f)
  {
    cout << f.path ()
         << ' ' << f.mtime ()
         << ' ' << f.version ()
         << ' ' << to_string (f.component ())
         << ' ' << f.size ();

    if (!f.hash ().empty ())
      cout << ' ' << f.hash ();

    cout << endl;
  }

  void
  print_files (vector<cached_file> fs)
  {
    ranges::sort (fs, {}, &cached_file::path);

    for (const cached_file& f: fs)
      print_file (f);
  }
}

// Usage: argv[0]
//
// Open the cache database of the installation in the current working
// directory, run the commands read from stdin, one per line, and print
// their output to stdout. The commands are:
//
//    store <path> <mtime> <version> <component> [<size> [<hash>]]
//    find <path>
//    erase <path>
//    erase-component <component>
//    files [<component>|<version>]
//    count [<component>]
//    version <component> [<tag>]
//    erase-version <component>
//    setting <key> [<value>]
//    erase-setting <key>
//    clear
//    check
//    hash <file>
//    verify <file> <hash>
//
// A file record prints in the form of the store arguments and the listings
// are sorted by path. A lookup that finds nothing prints 'none'. The hash
// command prints the Blake3 hash of a file and verify checks a file against
// one.
//
int
main ()
{
  try
  {
    cache_database db (filesystem::current_path ());

    for (string l; getline (cin, l); )
    {
      istringstream is (l);
      vector<string> as;

      for (string a; is >> a; )
        as.push_back (move (a));

      if (as.empty ())
        continue;

      const string& c (as[0]);
      size_t n (as.size ());

      auto value = [] (const optional<string>& v)
      {
        cout << (v ? *v : "none") << endl;
      };

      if (c == "store" && n >= 5 && n <= 7)
      {
        db.store (cached_file (as[1],
                               stoll (as[2]),
                               as[3],
                               to_component (as[4]),
                               n > 5 ? stoull (as[5]) : 0,
                               n > 6 ? as[6] : string ()));
      }
      else if (c == "find" && n == 2)
      {
        if (optional<cached_file> f = db.find (as[1]))
          print_file (*f);
        else
          cout << "none" << endl;
      }
      else if (c == "erase" && n == 2)
        db.erase (as[1]);
      else if (c == "erase-component" && n == 2)
        db.erase (to_component (as[1]));
      else if (c == "files" && n == 1)
        print_files (db.files ());
      else if (c == "files" && n == 2)
      {
        bool comp (ranges::find (components, as[1], [] (const auto& x)
        {
          return string (x.first);
        }) != ranges::end (components));

        print_files (comp ? db.files (to_component (as[1])) : db.files (as[1]));
      }
      else if (c == "count" && n <= 2)
      {
        size_t r (n == 1 ? db.count () : db.count (to_component (as[1])));
        cout << r << endl;
      }
      else if (c == "version" && n == 2)
      {
        optional<component_version> v (db.version (to_component (as[1])));
        value (v ? optional<string> (v->tag ()) : nullopt);
      }
      else if (c == "version" && n == 3)
        db.version (to_component (as[1]), as[2]);
      else if (c == "erase-version" && n == 2)
        db.erase_version (to_component (as[1]));
      else if (c == "setting" && n == 2)
      {
        optional<user_setting> s (db.setting (as[1]));
        value (s ? optional<string> (s->val ()) : nullopt);
      }
      else if (c == "setting" && n == 3)
        db.setting (as[1], as[2]);
      else if (c == "erase-setting" && n == 2)
        db.erase_setting (as[1]);
      else if (c == "clear" && n == 1)
        db.clear ();
      else if (c == "check" && n == 1)
        cout << boolalpha << db.check () << endl;
      else if (c == "hash" && n == 2)
      {
        blake3_hash h (blake3_hash::of_file (as[1]));
        value (!h.empty () ? optional<string> (h.string ()) : nullopt);
      }
      else if (c == "verify" && n == 3)
        cout << boolalpha << blake3_hash (as[2]).verify_file (as[1]) << endl;
      else
        fail ("invalid command '" + l + "'");
    }
  }
  catch (const exception& e)
  {
    fail (e.what ());
  }
}
