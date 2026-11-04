// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <vector>
#include <cstdlib>   // exit()
#include <utility>   // move()
#include <optional>
#include <iostream>
#include <exception>
#include <string_view>

#include <boost/asio/co_spawn.hpp>

#include <launcher/steam/steam-proton.hxx>

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

  // Print the Proton installations of the Steam installed in sr, best
  // first.
  //
  void
  print_versions (const fs::path& sr)
  {
    asio::io_context ioc;
    proton_manager pm (ioc);

    vector<proton_version> r;
    exception_ptr e;

    asio::co_spawn (ioc,
                    pm.detect_proton_versions (sr),
                    [&r, &e] (exception_ptr x, vector<proton_version> vs)
    {
      e = move (x);
      r = move (vs);
    });

    ioc.run ();

    if (e != nullptr)
      rethrow_exception (e);

    for (const proton_version& v: r)
      cout << v.name << endl;
  }
}

// Usage: argv[0] <command> [<argument>...]
//
// versions <steam-root>
//    Detect the Proton installations under <steam-root>/steamapps/common/
//    and print their names to stdout, one per line, in the order of
//    preference.
//
// helper-file <asset>
//    Print the name that the Steam helper's release asset is installed
//    under and nothing if the asset is not part of the helper.
//
// ghost-result <exit-status>
//    Print the outcome that the exit status of the Steam probe reports.
//
int
main (int argc, char* argv[])
{
  vector<string> as (argv + 1, argv + argc);
  string_view c (!as.empty () ? string_view (as[0]) : string_view ());

  try
  {
    if (c == "versions" && as.size () == 2)
    {
      print_versions (as[1]);
    }
    else if (c == "helper-file" && as.size () == 2)
    {
      if (optional<string> f = steam_helper_file (as[1]))
        cout << *f << endl;
    }
    else if (c == "ghost-result" && as.size () == 2)
    {
      cout << to_string (to_ghost_result (stoi (as[1]))) << endl;
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
