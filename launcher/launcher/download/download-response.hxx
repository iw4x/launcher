#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <optional>

#include <launcher/download/download-types.hxx>

namespace launcher
{
  // Download response represents the result of a download operation.
  //
  struct download_response
  {
    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;

    // State and progress.
    //
    download_state state {download_state::pending};
    download_progress progress;

    // Error information (if failed).
    //
    std::optional<download_error> error;

    // Timing information.
    //
    time_point start_time;
    time_point end_time;

    // Which URL was successfully used (index into request urls).
    //
    std::optional<std::size_t> successful_url_index;

    // HTTP-specific information.
    //
    std::optional<int> http_status_code;
    std::string content_type;
    std::optional<std::uint64_t> server_reported_size;

    // Constructors.
    //
    download_response () = default;

    explicit
    download_response (download_state st);

    // Status checks.
    //
    bool
    completed () const;

    bool
    failed () const;

    bool
    in_progress () const;

    // Duration calculation.
    //
    std::chrono::milliseconds
    duration () const;

    // Average speed calculation.
    //
    std::uint64_t
    average_speed_bps () const;
  };

  bool
  operator== (const download_response& x, const download_response& y);
}
