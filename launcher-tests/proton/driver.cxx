// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <vector>
#include <utility>   // move()
#include <iostream>
#include <exception>

#include <boost/asio/co_spawn.hpp>

#include <launcher/steam/steam-proton.hxx>

using namespace std;
using namespace launcher;

// Usage: argv[0] <steam-root>
//
// Detect the Proton installations under <steam-root>/steamapps/common/ and
// print their names to stdout, one per line, in the order of preference.
//
int
main (int argc, char* argv[])
{
  if (argc != 2)
  {
    cerr << "error: steam root argument expected" << endl;
    return 1;
  }

  // The coroutine refers to the Steam root, so keep it alive until the run
  // below returns.
  //
  fs::path sr (argv[1]);

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
