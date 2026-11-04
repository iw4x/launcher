// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <cstdlib>  // exit()
#include <utility>  // move()
#include <iostream>
#include <optional>
#include <string_view>

#include <launcher/update/update-types.hxx>

using namespace std;
using namespace launcher;

namespace
{
  // Return the parsed version. Fail if it is invalid.
  //
  launcher_version
  parse (const string& s)
  {
    optional<launcher_version> r (parse_launcher_version (s));

    if (!r)
    {
      cerr << "error: invalid version '" << s << "'" << endl;
      exit (1);
    }

    return move (*r);
  }

  // Return the kind of the version in the following form:
  //
  // (alpha <n>|beta <n>|release)[ snapshot]
  //
  string
  kind (const launcher_version& v)
  {
    string r (v.is_alpha () ? "alpha " + std::to_string (*v.alpha ()) :
              v.is_beta ()  ? "beta "  + std::to_string (*v.beta ())  :
                              "release");

    if (v.snapshot ())
      r += " snapshot";

    return r;
  }
}

// Usage: argv[0] (-p|-k|-c|-m)
//
// Read versions from stdin, one per line, and print the result for each to
// stdout, one per line. Fail on the first version that doesn't parse.
//
// -p
//    Print the canonical representation of the version.
//
// -k
//    Print the kind of the version (see kind()).
//
// -c
//    Read two space-separated versions per line and print the sign of their
//    comparison (-1 if the first is older).
//
// -m
//    Read the versions of a release asset and its tag, space-separated, per
//    line and print the version that the release resolves to (see
//    merge_revision()).
//
int
main (int argc, char* argv[])
{
  string_view m (argc == 2 ? argv[1] : "");

  if (m != "-p" && m != "-k" && m != "-c" && m != "-m")
  {
    cerr << "error: invalid arguments\n"
         << "  info: usage: driver (-p|-k|-c|-m)" << endl;
    return 1;
  }

  for (string l; getline (cin, l); )
  {
    if (m == "-c" || m == "-m")
    {
      size_t p (l.find (' '));

      if (p == string::npos)
      {
        cerr << "error: two versions expected in '" << l << "'" << endl;
        return 1;
      }

      launcher_version x (parse (l.substr (0, p)));
      launcher_version y (parse (l.substr (p + 1)));

      if (m == "-c")
      {
        int r (x.compare (y));
        cout << (r < 0 ? -1 : r > 0 ? 1 : 0) << endl;
      }
      else
        cout << merge_revision (move (x), y).string () << endl;
    }
    else
    {
      launcher_version v (parse (l));
      cout << (m == "-p" ? v.string () : kind (v)) << endl;
    }
  }
}
