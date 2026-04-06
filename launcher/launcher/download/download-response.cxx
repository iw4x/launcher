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
    // Easy enough. Just check if we've cleanly hit the completed state.
    //
    return state == download_state::completed;
  }

  bool
  download_response::failed () const
  {
    // Same deal here, but looking for an explicit failure.
    //
    return state == download_state::failed;
  }

  bool
  download_response::in_progress () const
  {
    // We consider a download active not just when we are pulling bytes, but
    // also during the initial connection handshake. Anything else is either an
    // initial or terminal state.
    //
    return state == download_state::connecting ||
           state == download_state::downloading;
  }

  chrono::milliseconds
  download_response::duration () const
  {
    // Bail out if either time point is default-constructed. This typically
    // means we haven't started or haven't finished yet, so calculating a
    // duration makes no sense here.
    //
    if (start_time == time_point () || end_time == time_point ())
      return chrono::milliseconds (0);

    return chrono::duration_cast<chrono::milliseconds> (
      end_time - start_time);
  }

  uint64_t
  download_response::average_speed_bps () const
  {
    auto d (duration ());

    // Watch out for division by zero. If the download was instantaneous or just
    // hasn't ticked a full millisecond yet, we report zero.
    //
    if (d.count () == 0)
      return 0;

    // Since our duration is in milliseconds, we need to scale the downloaded
    // bytes up by 1000 to get bytes per second. Do the multiplication first to
    // avoid precision loss from integer division truncation.
    //
    return (progress.downloaded_bytes * 1000) / d.count ();
  }

  bool
  operator== (const download_response& x, const download_response& y)
  {
    // Compare the core attributes. Notice that we intentionally ignore the
    // timing fields (start_time and end_time). Two responses are logically
    // equivalent if their state and byte counts match perfectly, even if they
    // happened at different times.
    //
    return x.state == y.state &&
           x.progress.downloaded_bytes == y.progress.downloaded_bytes &&
           x.progress.total_bytes == y.progress.total_bytes;
  }
}
