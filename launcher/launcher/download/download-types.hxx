#pragma once

#include <string>
#include <cstdint>
#include <utility>
#include <ostream>
#include <filesystem>

namespace launcher
{
  // Download state enumeration.
  //
  enum class download_state
  {
    pending,     // Not started yet
    connecting,  // Connecting to server
    downloading, // Actively downloading
    completed,   // Successfully completed
    failed,      // Failed with error
    paused       // Paused by user
  };

  std::ostream& operator<< (std::ostream& os, download_state s);

  // Download priority enumeration.
  //
  enum class download_priority
  {
    low,
    normal,
    high,
    critical
  };

  std::ostream& operator<< (std::ostream& os, download_priority p);

  // Download progress information.
  //
  struct download_progress
  {
    std::uint64_t total_bytes {0};      // Total size in bytes
    std::uint64_t downloaded_bytes {0}; // Downloaded so far
    std::uint64_t speed_bps {0};        // Current speed in bytes/sec
    double progress_percent {0.0};      // Progress percentage (0-100)

    download_progress () = default;

    download_progress (std::uint64_t total,
                       std::uint64_t downloaded,
                       std::uint64_t speed = 0);

    bool completed () const;
  };

  std::ostream& operator<< (std::ostream& os, const download_progress& p);

  // Download error information.
  //
  struct download_error
  {
    std::string message;
    std::string url;
    int error_code {0};

    download_error () = default;

    download_error (std::string msg, std::string u = "", int code = 0);

    bool empty () const;
  };

  std::ostream& operator<< (std::ostream& os, const download_error& e);
}
