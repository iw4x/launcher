// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <vector>
#include <utility>    // move()
#include <iostream>
#include <exception>
#include <filesystem>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>

#include <launcher/launcher-manifest.hxx>

using namespace std;
using namespace launcher;

// Usage: argv[0] <archive>
//
// Extract the archive into the current working directory and print the
// extracted files relative to it, one per line. A file with any of its
// execute bits set is followed by ' executable'.
//
int
main (int argc, char* argv[])
{
  if (argc != 2)
  {
    cerr << "error: archive argument expected" << endl;
    return 1;
  }

  // The coroutine refers to these, so keep them alive until the run below
  // returns.
  //
  const fs::path ap (argv[1]);
  const fs::path d (fs::current_path ());

  manifest_archive a;
  a.name = ap.filename ().string ();

  vector<fs::path> r;
  exception_ptr e;

  asio::io_context ioc;
  asio::co_spawn (ioc,
                  manifest_coordinator::extract_archive (a, ap, d),
                  [&r, &e] (exception_ptr x, vector<fs::path> fs)
  {
    e = move (x);
    r = move (fs);
  });

  ioc.run ();

  if (e != nullptr)
    rethrow_exception (e);

  const fs::perms x (fs::perms::owner_exec |
                     fs::perms::group_exec |
                     fs::perms::others_exec);

  for (const fs::path& f: r)
  {
    bool ex ((fs::status (f).permissions () & x) != fs::perms::none);

    cout << fs::relative (f, d).generic_string ();

    if (ex)
      cout << " executable";

    cout << endl;
  }
}
