// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <cstdint>
#include <utility>
#include <ostream>
#include <filesystem>

namespace launcher
{
  // State of a download task.
  //
  enum class download_state
  {
    pending,     // Not started yet.
    connecting,  // Connecting to the server.
    downloading, // Receiving the data.
    completed,   // Downloaded successfully.
    failed,      // Failed, see the response error.
    paused       // Paused at the caller's request.
  };

  std::ostream& operator<< (std::ostream& os, download_state s);

  // Priority of a download task. Tasks of higher priority start first.
  //
  enum class download_priority
  {
    low,
    normal,
    high,
    critical
  };

  std::ostream& operator<< (std::ostream& os, download_priority p);

  // Progress of a download. The total is 0 if it is not known (for example,
  // if the server didn't send the content length).
  //
  struct download_progress
  {
    std::uint64_t total_bytes {0};
    std::uint64_t downloaded_bytes {0};
    std::uint64_t speed_bps {0};        // Bytes per second.
    double progress_percent {0.0};      // From 0 to 100, 0 if no total.

    download_progress () = default;

    // Calculate the percentage from the totals.
    //
    download_progress (std::uint64_t total,
                       std::uint64_t downloaded,
                       std::uint64_t speed = 0);

    // Return true if the total is known and reached.
    //
    bool completed () const;
  };

  std::ostream& operator<< (std::ostream& os, const download_progress& p);

  // Download failure with the URL that it happened on, if any.
  //
  struct download_error
  {
    std::string message;
    std::string url;
    int error_code {0};

    download_error () = default;

    download_error (std::string msg, std::string u = "", int code = 0);

    // Return true if there is no message.
    //
    bool empty () const;
  };

  std::ostream& operator<< (std::ostream& os, const download_error& e);
}
