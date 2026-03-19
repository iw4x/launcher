#include <launcher/download/download-task.hxx>

using namespace std;

namespace launcher
{
  download_task::download_task (download_request r)
    : request (move (r))
  {
  }

  download_task::download_task (download_request r, default_download_handler h)
    : request (move (r)),
      handler (move (h))
  {
  }

  void
  download_task::set_state (download_state s)
  {
    // Atomically exchange the state. If we actually transitioned and have a
    // callback registered, let the observer know.
    //
    download_state os (state.exchange (s));

    if (os != s && on_state_change)
      on_state_change (os, s);

    // Keep the response structure in sync with our current state.
    //
    response.state = s;
  }

  void
  download_task::update_progress (uint64_t d, uint64_t t)
  {
    downloaded_bytes.store (d);

    // Only overwrite the total if we actually know it. A zero usually means the
    // server hasn't sent a Content-Length yet.
    //
    if (t > 0)
      total_bytes.store (t);

    response.progress.downloaded_bytes = d;

    // Fallback to the previously loaded total if the provided one is zero.
    //
    response.progress.total_bytes = t > 0 ? t : total_bytes.load ();

    if (on_progress)
      on_progress (response.progress);
  }

  void
  download_task::set_error (download_error e)
  {
    // Stash the error in the response and immediately transition the task to
    // the failed state.
    //
    response.error = move (e);
    set_state (download_state::failed);
  }

  bool
  download_task::completed () const
  {
    return state.load () == download_state::completed;
  }

  bool
  download_task::failed () const
  {
    return state.load () == download_state::failed;
  }

  bool
  download_task::active () const
  {
    // A task is considered active if we are either trying to connect or already
    // moving bytes around.
    //
    download_state s (state.load ());
    return s == download_state::connecting ||
           s == download_state::downloading;
  }

  bool
  download_task::should_cancel () const
  {
    return cancel_requested.load ();
  }

  bool
  download_task::should_pause () const
  {
    return pause_requested.load ();
  }

  void
  download_task::cancel ()
  {
    // Just set the flag. The actual worker thread needs to notice this on its
    // next iteration and bail out.
    //
    cancel_requested.store (true);
  }

  void
  download_task::pause ()
  {
    pause_requested.store (true);
  }

  void
  download_task::resume ()
  {
    // Clear the pause flag first. If the worker actually managed to park
    // itself, bump the state back to pending so the scheduler picks it up
    // again.
    //
    pause_requested.store (false);

    if (state.load () == download_state::paused)
      set_state (download_state::pending);
  }

  shared_ptr<download_task>
  make_download_task (download_request r)
  {
    return make_shared<download_task> (move (r));
  }

  shared_ptr<download_task>
  make_download_task (download_request r, default_download_handler h)
  {
    return make_shared<download_task> (move (r), move (h));
  }
}
