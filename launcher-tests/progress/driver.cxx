// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <print>
#include <string>
#include <vector>
#include <iostream>
#include <exception>

#include <launcher/progress/progress-tracker.hxx>

using namespace std;
using namespace launcher;

// Usage: argv[0] <command> <argument>...
//
// Format the arguments for the progress display and print the result. The
// commands are:
//
//    bytes <count>
//    speed <bytes-per-second>
//    duration <seconds>
//    bar <progress> <width> [<frame>]
//    indeterminate <width>
//
// The progress is a ratio from 0 to 1. At no progress the bar shows the idle
// animation, with the frame selecting its step.
//
int
main (int argc, char* argv[])
{
  vector<string> as (argv + 1, argv + argc);
  const string c (!as.empty () ? as[0] : "");
  const size_t n (as.size ());

  try
  {
    using pt = progress_tracker;

    if (c == "bytes" && n == 2)
      println ("{}", pt::format_bytes (stoull (as[1])));
    else if (c == "speed" && n == 2)
      println ("{}", pt::format_speed (stof (as[1])));
    else if (c == "duration" && n == 2)
      println ("{}", pt::format_duration (stoi (as[1])));
    else if (c == "bar" && (n == 3 || n == 4))
      println ("{}", pt::format_bar (stof (as[1]),
                                     false,
                                     stoi (as[2]),
                                     n == 4 ? stoul (as[3]) : 0));
    else if (c == "indeterminate" && n == 2)
      println ("{}", pt::format_bar (0, true, stoi (as[1])));
    else
    {
      println (cerr,
               "error: invalid arguments\n"
               "  info: usage: driver <command> <argument>...");
      return 1;
    }
  }
  catch (const exception& e)
  {
    println (cerr, "error: {}", e.what ());
    return 1;
  }
}
