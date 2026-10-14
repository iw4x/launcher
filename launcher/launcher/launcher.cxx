// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <print>    // println()
#include <sstream>  // ostringstream
#include <iostream> // cout, cerr

#include <launcher/version.hxx>
#include <launcher/launcher-options.hxx>

using namespace std;

namespace launcher
{
  namespace
  {
    int
    main (int argc, char* argv[])
    try
    {
      // Parse the command line. We don't take any arguments (yet) so treat
      // them as errors, the same as unknown options.
      //
      options o;
      o.parse (argc, argv,
               false /* erase */,
               cli::unknown_mode::fail,
               cli::unknown_mode::fail);

      // Handle --version.
      //
      if (o.version ())
      {
        println ("launcher {}\n"
                 "Copyright (c) the IW4x authors.\n"
                 "This is free software released under the GNU General Public "
                 "License, version 3,\n"
                 "with the IW4x Linking Exception, version 1.1.",
                 LAUNCHER_VERSION_ID);

        return 0;
      }

      // Handle --help.
      //
      if (o.help ())
      {
        print_launcher_usage (cout);
        return 0;
      }

      return 0;
    }
    catch (const cli::exception& e)
    {
      // Note that cli::exception can only be printed to a stream so format
      // it into a string first.
      //
      ostringstream os;
      os << e;

      println (cerr,
               "error: {}\n"
               "  info: run 'launcher --help' for more information",
               os.str ());

      return 1;
    }
  }
}

int
main (int argc, char* argv[])
{
  return launcher::main (argc, argv);
}
