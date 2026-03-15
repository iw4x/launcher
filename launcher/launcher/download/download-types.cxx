#include <launcher/download/download-types.hxx>

namespace launcher
{
  std::ostream&
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

  std::ostream&
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

  download_progress::download_progress (std::uint64_t total,
                                        std::uint64_t downloaded,
                                        std::uint64_t speed)
    : total_bytes (total),
      downloaded_bytes (downloaded),
      speed_bps (speed),
      progress_percent (total > 0 ? (downloaded * 100.0) / total : 0.0)
  {
  }

  bool
  download_progress::completed () const
  {
    return total_bytes > 0 && downloaded_bytes >= total_bytes;
  }

  std::ostream&
  operator<< (std::ostream& os, const download_progress& p)
  {
    return os << p.downloaded_bytes << '/' << p.total_bytes
              << " (" << p.progress_percent << "%)";
  }

  download_error::download_error (std::string msg, std::string u, int code)
    : message (std::move (msg)),
      url (std::move (u)),
      error_code (code)
  {
  }

  bool
  download_error::empty () const
  {
    return message.empty ();
  }

  std::ostream&
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
