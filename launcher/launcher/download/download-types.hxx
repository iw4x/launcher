#pragma once

#include <string>
#include <cstdint>
#include <utility>
#include <ostream>
#include <filesystem>

namespace launcher
{
  namespace fs = std::filesystem;

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

  inline std::ostream&
  operator<< (std::ostream& os, download_state s)
  {
    switch (s)
    {
    case download_state::pending:     return os << "pending";
    case download_state::connecting:  return os << "connecting";
    case download_state::downloading: return os << "downloading";
    case download_state::completed:   return os << "completed";
    case download_state::failed:      return os << "failed";
    case download_state::paused:      return os << "paused";
    }
    return os;
  }

  // Download priority enumeration.
  //
  enum class download_priority
  {
    low,
    normal,
    high,
    critical
  };

  inline std::ostream&
  operator<< (std::ostream& os, download_priority p)
  {
    switch (p)
    {
      case download_priority::low:      return os << "low";
      case download_priority::normal:   return os << "normal";
      case download_priority::high:     return os << "high";
      case download_priority::critical: return os << "critical";
    }
    return os;
  }

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
                       std::uint64_t speed = 0)
      : total_bytes (total),
        downloaded_bytes (downloaded),
        speed_bps (speed),
        progress_percent (total > 0 ? (downloaded * 100.0) / total : 0.0)
    {
    }

    bool
    completed () const
    {
      return total_bytes > 0 && downloaded_bytes >= total_bytes;
    }
  };

  inline std::ostream&
  operator<< (std::ostream& os, const download_progress& p)
  {
    return os << p.downloaded_bytes << '/' << p.total_bytes
              << " (" << p.progress_percent << "%)";
  }

  // Download error information.
  //
  struct download_error
  {
    std::string message;
    std::string url;
    int error_code {0};

    download_error () = default;

    download_error (std::string msg, std::string u = "", int code = 0)
      : message (std::move (msg)),
        url (std::move (u)),
        error_code (code)
    {
    }

    bool
    empty () const
    {
      return message.empty ();
    }
  };

  inline std::ostream&
  operator<< (std::ostream& os, const download_error& e)
  {
    os << e.message;
    if (!e.url.empty ())
      os << " [url: " << e.url << "]";
    if (e.error_code != 0)
      os << " [code: " << e.error_code << "]";
    return os;
  }
}
