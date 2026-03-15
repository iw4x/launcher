#include <launcher/download/download-response.hxx>

namespace launcher
{
  download_response::download_response (download_state st)
    : state (st)
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

  std::chrono::milliseconds
  download_response::duration () const
  {
    if (start_time == time_point () || end_time == time_point ())
      return std::chrono::milliseconds (0);

    return std::chrono::duration_cast<std::chrono::milliseconds> (
        end_time - start_time);
  }

  std::uint64_t
  download_response::average_speed_bps () const
  {
    auto dur (duration ());
    if (dur.count () == 0)
      return 0;

    return (progress.downloaded_bytes * 1000) / dur.count ();
  }

  bool
  operator== (const download_response& x, const download_response& y)
  {
    return x.state == y.state &&
           x.progress.downloaded_bytes == y.progress.downloaded_bytes &&
           x.progress.total_bytes == y.progress.total_bytes;
  }

  bool
  operator!= (const download_response& x, const download_response& y)
  {
    return !(x == y);
  }
}
