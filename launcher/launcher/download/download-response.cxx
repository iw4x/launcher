// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/download/download-response.hxx>

using namespace std;

namespace launcher
{
  download_response::download_response (download_state s)
    : state (s)
  {
  }

  bool
  download_response::completed () const
  {
    return state == download_state::completed;
  }

  bool
  download_response::failed () const
  {
    return state == download_state::failed;
  }

  bool
  download_response::in_progress () const
  {
    return state == download_state::connecting ||
           state == download_state::downloading;
  }

  chrono::milliseconds
  download_response::duration () const
  {
    if (start_time == time_point () || end_time == time_point ())
      return chrono::milliseconds (0);

    return chrono::duration_cast<chrono::milliseconds> (
      end_time - start_time);
  }

  uint64_t
  download_response::average_speed_bps () const
  {
    auto d (duration ());

    if (d.count () == 0)
      return 0;

    // Note that we multiply before dividing to keep the precision.
    //
    return (progress.downloaded_bytes * 1000) / d.count ();
  }

  bool
  operator== (const download_response& x, const download_response& y)
  {
    return x.state == y.state &&
           x.progress.downloaded_bytes == y.progress.downloaded_bytes &&
           x.progress.total_bytes == y.progress.total_bytes;
  }
}
