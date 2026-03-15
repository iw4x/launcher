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
  // Default handler type.
  //
  struct default_download_handler {};

  // Download task abstraction.
  //
  class download_task
  {
  public:
    using progress_callback = std::function<void (const download_progress&)>;
    using state_callback = std::function<void (download_state, download_state)>;

    // Constructors.
    //
    download_task () = default;

    explicit
    download_task (download_request req);

    download_task (download_request req, default_download_handler hdl);

    // Request and response.
    //
    download_request request;
    download_response response;

    // Handler (customizable per-task behavior).
    //
    default_download_handler handler;

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
    set_state (download_state new_state);

    void
    update_progress (std::uint64_t downloaded, std::uint64_t total = 0);

    void
    set_error (download_error err);

    // Status checks.
    //
    bool
    completed () const;

    bool
    failed () const;

    bool
    active () const;

    bool
    should_cancel () const;

    bool
    should_pause () const;

    // Control operations.
    //
    void
    cancel ();

    void
    pause ();

    void
    resume ();
  };

  // Convenience factory functions.
  //
  std::shared_ptr<download_task>
  make_download_task (download_request req);

  std::shared_ptr<download_task>
  make_download_task (download_request req, default_download_handler hdl);
}
