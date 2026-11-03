// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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
  // Per-task download behavior. Currently there is nothing to customize.
  //
  struct default_download_handler {};

  // Download of a single request, shared between the manager that performs
  // it and the code that observes it.
  //
  class download_task
  {
  public:
    using progress_callback = std::function<void (const download_progress&)>;

    // Called with the old and the new state.
    //
    using state_callback = std::function<void (download_state, download_state)>;

  public:
    download_task () = default;

    explicit
    download_task (download_request req);

    download_task (download_request req, default_download_handler hdl);

  public:
    download_request request;
    download_response response;

    default_download_handler handler;

    progress_callback on_progress;
    state_callback on_state_change;

    std::atomic<download_state> state {download_state::pending};
    std::atomic<std::uint64_t> downloaded_bytes {0};
    std::atomic<std::uint64_t> total_bytes {0};

    std::atomic<bool> cancel_requested {false};
    std::atomic<bool> pause_requested {false};

  public:
    // Progress.
    //
    // Set the state of the task and the response and call on_state_change if
    // the state changed.
    //
    void
    set_state (download_state new_state);

    // Set the byte counts of the task and the response and call on_progress.
    // A total of 0 means unknown, in which case we keep the last known total.
    //
    void
    update_progress (std::uint64_t downloaded, std::uint64_t total = 0);

    // Store the error in the response and change the state to failed.
    //
    void
    set_error (download_error err);

    bool
    completed () const;

    bool
    failed () const;

    // Return true if connecting or downloading.
    //
    bool
    active () const;

  public:
    // Control.
    //
    // Note that these functions only request the change. The download checks
    // the requests between attempts and the cancellation request also as it
    // receives data.
    //
    bool
    should_cancel () const;

    bool
    should_pause () const;

    void
    cancel ();

    void
    pause ();

    // Clear the pause request and, if the download is paused, change the
    // state back to pending.
    //
    void
    resume ();
  };
  std::shared_ptr<download_task>
  make_download_task (download_request req);

  std::shared_ptr<download_task>
  make_download_task (download_request req, default_download_handler hdl);
}
