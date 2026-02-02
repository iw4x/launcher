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
  template <typename S = std::string>
  struct basic_download_response
  {
    using string_type = S;
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
    string_type content_type;
    std::optional<std::uint64_t> server_reported_size;

    // Constructors.
    //
    basic_download_response () = default;

    explicit
    basic_download_response (download_state st)
      : state (st)
    {
    }

    // Status checks.
    //
    bool
    completed () const
    {
      return state == download_state::completed;
    }

    bool
    failed () const
    {
      return state == download_state::failed;
    }

    bool
    in_progress () const
    {
      return state == download_state::connecting ||
             state == download_state::downloading;
    }

    // Duration calculation.
    //
    std::chrono::milliseconds
    duration () const
    {
      if (start_time == time_point () || end_time == time_point ())
        return std::chrono::milliseconds (0);

      return std::chrono::duration_cast<std::chrono::milliseconds> (
          end_time - start_time);
    }

    // Average speed calculation.
    //
    std::uint64_t
    average_speed_bps () const
    {
      auto dur (duration ());
      if (dur.count () == 0)
        return 0;

      return (progress.downloaded_bytes * 1000) / dur.count ();
    }
  };

  using download_response = basic_download_response<std::string>;

  template <typename S>
  inline bool
  operator== (const basic_download_response<S>& x,
              const basic_download_response<S>& y)
  {
    return x.state == y.state &&
           x.progress.downloaded_bytes == y.progress.downloaded_bytes &&
           x.progress.total_bytes == y.progress.total_bytes;
  }

  template <typename S>
  inline bool
  operator!= (const basic_download_response<S>& x,
              const basic_download_response<S>& y)
  {
    return !(x == y);
  }
}
