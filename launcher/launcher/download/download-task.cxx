// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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
    download_state os (state.exchange (s));

    if (os != s && on_state_change)
      on_state_change (os, s);

    response.state = s;
  }

  void
  download_task::update_progress (uint64_t d, uint64_t t)
  {
    downloaded_bytes.store (d);

    // A total of 0 means that the server didn't send the content length, so
    // keep the one that we know, if any.
    //
    if (t > 0)
      total_bytes.store (t);

    response.progress.downloaded_bytes = d;
    response.progress.total_bytes = t > 0 ? t : total_bytes.load ();

    if (on_progress)
      on_progress (response.progress);
  }

  void
  download_task::set_error (download_error e)
  {
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
