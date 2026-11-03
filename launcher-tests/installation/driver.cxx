// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <print>
#include <string>
#include <vector>
#include <iostream>
#include <exception>
#include <filesystem>

#include <launcher/launcher-installation.hxx>

using namespace std;
using namespace launcher;

// Usage: argv[0] <command> [<argument>...]
//
// Run the command on the installation in the current working directory and
// print its result, if any, to stdout. Print the error of a command that
// throws to stderr and exit with 1.
//
// migrate-clients
//    Rename the clients installed under their legacy names.
//
// migrate-dlc <file>...
//    Move the DLC files installed under zone/dlc/ to the specified
//    destinations, relative to the installation root.
//
// retire <file>
//    Move the file into the retired directory and print whether it moved.
//
// remove-retired
//    Remove the retired files.
//
// reset-cache <version>
//    Wipe the cache of another launcher version and print whether the
//    version changed.
//
int
main (int argc, char* argv[])
try
{
  auto usage = [] ()
  {
    println (cerr,
             "error: invalid arguments\n"
             "  info: usage: driver <command> [<argument>...]");
    return 1;
  };

  if (argc < 2)
    return usage ();

  const string c (argv[1]);
  const bool a (argc == 3);

  installation ins (filesystem::current_path ());

  if (c == "migrate-clients" && argc == 2)
    ins.migrate_clients ();
  else if (c == "migrate-dlc" && argc >= 3)
  {
    vector<filesystem::path> fs;

    for (int i (2); i != argc; ++i)
      fs.push_back (ins.root () / argv[i]);

    ins.migrate_dlc (fs);
  }
  else if (c == "retire" && a)
    println ("{}", ins.retire (argv[2]));
  else if (c == "remove-retired" && argc == 2)
    ins.remove_retired ();
  else if (c == "reset-cache" && a)
    println ("{}", ins.reset_cache (argv[2]));
  else
    return usage ();
}
catch (const exception& e)
{
  println (cerr, "error: {}", e.what ());
  return 1;
}
