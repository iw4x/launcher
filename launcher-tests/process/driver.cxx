// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <errno.h>
#include <sys/wait.h>

#include <print>
#include <string>
#include <vector>
#include <iostream>

#include <launcher/process.hxx>

using namespace std;
using namespace launcher;

// Usage: argv[0] <program> [<argument>...]
//
// Start the program detached and print whether we have any child process
// afterwards, which a started program that exits would leave behind until
// its parent waits for it.
//
int
main (int argc, char* argv[])
{
  if (argc < 2)
  {
    println (cerr, "error: program expected");
    return 1;
  }

  int r (0);

  try
  {
    start_detached (argv[1], vector<string> (argv + 2, argv + argc));
  }
  catch (const process_error& e)
  {
    println (cerr, "error: {}", e.what ());
    r = 1;
  }

  int s;
  pid_t p (waitpid (-1, &s, WNOHANG));

  println ("{}", p == -1 && errno == ECHILD ? "no children" : "children");

  return r;
}
