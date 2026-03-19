#include <launcher/download/download-types.hxx>

using namespace std;

namespace launcher
{
  ostream&
  operator<< (ostream& os, download_state s)
  {
    // Omit the default case to let the compiler warn us about unhandled
    // enumerators. Saves us from forgetting to update this when a new state is
    // added.
    //
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

  ostream&
  operator<< (ostream& os, download_priority p)
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

  download_progress::download_progress (uint64_t t,
                                        uint64_t d,
                                        uint64_t s)
    : total_bytes (t),
      downloaded_bytes (d),
      speed_bps (s),
      progress_percent (t > 0 ? (d * 100.0) / t : 0.0)
  {
    // Compute the percentage upfront so we don't have to do it on the fly
    // everywhere. Make sure to guard against a zero total, which is common for
    // chunked transfers and missing content length headers.
    //
  }

  bool
  download_progress::completed () const
  {
    // We use >= rather than == since the actual downloaded amount might
    // overshoot the advertised total if the server sent a bad header.
    //
    return total_bytes > 0 && downloaded_bytes >= total_bytes;
  }

  ostream&
  operator<< (ostream& os, const download_progress& p)
  {
    return os << p.downloaded_bytes << '/' << p.total_bytes
              << " (" << p.progress_percent << "%)";
  }

  download_error::download_error (string m, string u, int c)
    : message (move (m)),
      url (move (u)),
      error_code (c)
  {
  }

  bool
  download_error::empty () const
  {
    // Consider the error empty if there is no message. The url and code
    // are merely auxiliary context.
    //
    return message.empty ();
  }

  ostream&
  operator<< (ostream& os, const download_error& e)
  {
    os << e.message;

    // Only print the URL and code if we actually have them to avoid cluttering
    // the output with empty brackets or zeros.
    //
    if (!e.url.empty ())
      os << " [url: " << e.url << "]";

    if (e.error_code != 0)
      os << " [code: " << e.error_code << "]";

    return os;
  }
}
