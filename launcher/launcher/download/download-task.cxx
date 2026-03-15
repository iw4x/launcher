#include <launcher/download/download-task.hxx>

namespace launcher
{
  download_task::download_task (download_request req)
    : request (std::move (req))
  {
  }

  download_task::download_task (download_request req, default_download_handler hdl)
    : request (std::move (req)),
      handler (std::move (hdl))
  {
  }

  void
  download_task::set_state (download_state new_state)
  {
    download_state old_state (state.exchange (new_state));
    if (old_state != new_state && on_state_change)
      on_state_change (old_state, new_state);

    response.state = new_state;
  }

  void
  download_task::update_progress (std::uint64_t downloaded, std::uint64_t total)
  {
    downloaded_bytes.store (downloaded);
    if (total > 0)
      total_bytes.store (total);

    response.progress.downloaded_bytes = downloaded;
    response.progress.total_bytes = total > 0 ? total : total_bytes.load ();

    if (on_progress)
      on_progress (response.progress);
  }

  void
  download_task::set_error (download_error err)
  {
    response.error = std::move (err);
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
    download_state st (state.load ());
    return st == download_state::connecting ||
           st == download_state::downloading;
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

  std::shared_ptr<download_task>
  make_download_task (download_request req)
  {
    return std::make_shared<download_task> (std::move (req));
  }

  std::shared_ptr<download_task>
  make_download_task (download_request req, default_download_handler hdl)
  {
    return std::make_shared<download_task> (std::move (req), std::move (hdl));
  }
}
