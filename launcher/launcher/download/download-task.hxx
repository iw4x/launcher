#pragma once

#include <memory>
#include <atomic>
#include <functional>

#include <boost/asio.hpp>

#include <launcher/download/download-request.hxx>
#include <launcher/download/download-response.hxx>
#include <launcher/download/download-types.hxx>

namespace launcher
{
  // Forward declarations.
  //
  template <typename H, typename T> class basic_download_task;

  // Download task traits.
  //
  template <typename H, typename S = H>
  struct download_task_traits
  {
    using handler_type = H;
    using string_type = S;

    using request_type  = basic_download_request<string_type>;
    using response_type = basic_download_response<string_type>;

    // Progress callback type.
    //
    using progress_callback = std::function<void (const download_progress&)>;

    // State change callback type.
    //
    using state_callback = std::function<void (download_state, download_state)>;
  };

  // Basic download task abstraction.
  //
  template <typename H, typename T = download_task_traits<H>>
  class basic_download_task
  {
  public:
    using traits_type = T;
    using handler_type = typename traits_type::handler_type;
    using string_type = typename traits_type::string_type;
    using request_type = typename traits_type::request_type;
    using response_type = typename traits_type::response_type;
    using progress_callback = typename traits_type::progress_callback;
    using state_callback = typename traits_type::state_callback;

    // Constructors.
    //
    basic_download_task () = default;

    explicit
    basic_download_task (request_type req)
      : request (std::move (req))
    {
    }

    basic_download_task (request_type req, handler_type hdl)
      : request (std::move (req)),
        handler (std::move (hdl))
    {
    }

    // Request and response.
    //
    request_type request;
    response_type response;

    // Handler (customizable per-task behavior).
    //
    handler_type handler;

    // Callbacks.
    //
    progress_callback on_progress;
    state_callback on_state_change;

    // Atomic state tracking.
    //
    std::atomic<download_state> state {download_state::pending};
    std::atomic<std::uint64_t> downloaded_bytes {0};
    std::atomic<std::uint64_t> total_bytes {0};

    // Control.
    //
    std::atomic<bool> cancel_requested {false};
    std::atomic<bool> pause_requested {false};

    // State management.
    //
    void
    set_state (download_state new_state)
    {
      download_state old_state (state.exchange (new_state));
      if (old_state != new_state && on_state_change)
        on_state_change (old_state, new_state);

      response.state = new_state;
    }

    void
    update_progress (std::uint64_t downloaded, std::uint64_t total = 0)
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
    set_error (download_error err)
    {
      response.error = std::move (err);
      set_state (download_state::failed);
    }

    // Status checks.
    //
    bool
    completed () const
    {
      return state.load () == download_state::completed;
    }

    bool
    failed () const
    {
      return state.load () == download_state::failed;
    }

    bool
    active () const
    {
      download_state st (state.load ());
      return st == download_state::connecting ||
             st == download_state::downloading;
    }

    bool
    should_cancel () const
    {
      return cancel_requested.load ();
    }

    bool
    should_pause () const
    {
      return pause_requested.load ();
    }

    // Control operations.
    //
    void
    cancel ()
    {
      cancel_requested.store (true);
    }

    void
    pause ()
    {
      pause_requested.store (true);
    }

    void
    resume ()
    {
      pause_requested.store (false);
      if (state.load () == download_state::paused)
        set_state (download_state::pending);
    }
  };

  // Default handler type and task.
  //
  struct default_download_handler {};

  using download_task =
      basic_download_task<default_download_handler>;

  // Convenience factory functions.
  //
  template <typename H, typename T>
  inline std::shared_ptr<basic_download_task<H, T>>
  make_download_task (typename basic_download_task<H, T>::request_type req)
  {
    return std::make_shared<basic_download_task<H, T>> (std::move (req));
  }

  template <typename H, typename T>
  inline std::shared_ptr<basic_download_task<H, T>>
  make_download_task (typename basic_download_task<H, T>::request_type req,
                      H hdl)
  {
    return std::make_shared<basic_download_task<H, T>> (
        std::move (req), std::move (hdl));
  }
}

#include <launcher/download/download-task.txx>
