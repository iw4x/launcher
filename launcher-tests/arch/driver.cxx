// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <iostream>
#include <optional>
#include <string_view>

#include <launcher/arch/arch-types.hxx>

using namespace std;
using namespace launcher;

// Usage: argv[0] [-e|-l]
//
// Parse the architecture names read from stdin, one per line, and print the
// canonical name of each to stdout. Fail on the first name that doesn't
// parse.
//
// -e
//    Print the architecture's client executable.
//
// -l
//    Print the architecture's label.
//
int
main (int argc, char* argv[])
{
  using print_function = string_view (architecture) noexcept;

  print_function* print (&to_string);

  if (argc == 2 && argv[1] == string_view ("-e"))
    print = &architecture_executable;
  else if (argc == 2 && argv[1] == string_view ("-l"))
    print = &architecture_label;
  else if (argc != 1)
  {
    cerr << "error: invalid arguments\n"
         << "  info: usage: driver [-e|-l]" << endl;
    return 1;
  }

  for (string l; getline (cin, l); )
  {
    optional<architecture> a (parse_architecture (l));

    if (!a)
    {
      cerr << "error: invalid architecture '" << l << "'" << endl;
      return 1;
    }

    cout << print (*a) << endl;
  }
}
