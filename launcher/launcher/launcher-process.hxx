// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <format>
#include <string>
#include <vector>
#include <utility>      // forward()
#include <type_traits>  // remove_cvref_t
#include <system_error>

namespace launcher
{
  // An argument that std::format() can format.
  //
  template <typename T>
  concept format_argument = std::formattable<std::remove_cvref_t<T>, char>;

  // Error of starting a program, with the error code and the description of
  // what we were doing, formatted from the format string and its arguments,
  // for example:
  //
  // throw process_error (e, "unable to execute {}", p);
  //
  // The code is either an errno value or, on Windows, a system error code
  // (see GetLastError()).
  //
  struct process_error: std::system_error
  {
    template <format_argument... A>
    process_error (int e, std::format_string<A...> f, A&&... a)
      : system_error (e,
                      std::generic_category (),
                      std::format (f, std::forward<A> (a)...))
    {
    }

    template <format_argument... A>
    process_error (std::error_code e, std::format_string<A...> f, A&&... a)
      : system_error (e, std::format (f, std::forward<A> (a)...))
    {
    }
  };

  // Start the program with the arguments and don't wait for it. Search for
  // the program in PATH unless it contains a directory separator.
  //
  // The program runs in a session (process group on Windows) of its own,
  // with its standard streams connected to the null device, and outlives
  // us. Throw process_error if unable to start it, including if it doesn't
  // exist or cannot be executed.
  //
  // Note that on POSIX the program is our grandchild, which init (or the
  // closest subreaper) adopts once its parent, our child, exits. Otherwise,
  // once the program exits it would remain our zombie until we wait for it.
  // And other tools that look for a running program by its name take such a
  // zombie for one. For example, Companion would wait for a Steam client
  // that is long gone.
  //
  // Also note that the program inherits our descriptors that are not
  // close-on-exec and that the signals that we handle or ignore are reset to
  // their defaults for it (and those that we block, unblocked).
  //
  void
  start_detached (const std::string& program,
                  const std::vector<std::string>& args);
}
