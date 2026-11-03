// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <optional>

#include <launcher/download/download-types.hxx>

namespace launcher
{
  // Outcome of a download, filled in as the download progresses.
  //
  struct download_response
  {
    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;

    download_state state {download_state::pending};
    download_progress progress;

    std::optional<download_error> error; // Present if failed.

    // Start and end times, default-constructed until known.
    //
    time_point start_time;
    time_point end_time;

    // Index of the request URL that the download succeeded from.
    //
    std::optional<std::size_t> successful_url_index;

    std::optional<int> http_status_code;
    std::string content_type;
    std::optional<std::uint64_t> server_reported_size;

    download_response () = default;

    explicit
    download_response (download_state st);

    bool
    completed () const;

    bool
    failed () const;

    // Return true if connecting or downloading.
    //
    bool
    in_progress () const;

    // Return the time between the start and the end or 0 if either is not
    // known yet.
    //
    std::chrono::milliseconds
    duration () const;

    // Return the average speed in bytes per second or 0 if the duration is 0.
    //
    std::uint64_t
    average_speed_bps () const;
  };

  // Note that two responses compare equal if their states and byte counts
  // are equal.
  //
  bool
  operator== (const download_response& x, const download_response& y);
}
