// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/download/download-types.hxx>

using namespace std;

namespace launcher
{
  ostream&
  operator<< (ostream& os, download_state s)
  {
    // Note: no default so that the compiler warns about a new state.
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
  }

  bool
  download_progress::completed () const
  {
    // Note that a server can send less than it actually transfers in the
    // content length, so we can overshoot the total.
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
    return message.empty ();
  }

  ostream&
  operator<< (ostream& os, const download_error& e)
  {
    os << e.message;

    // Print the URL and the code only if we have them.
    //
    if (!e.url.empty ())
      os << " [url: " << e.url << "]";

    if (e.error_code != 0)
      os << " [code: " << e.error_code << "]";

    return os;
  }
}
