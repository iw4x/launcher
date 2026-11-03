// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <launcher/progress/progress.hxx>

#include <boost/asio.hpp>

#include <string>
#include <functional>
#include <vector>

namespace launcher
{
  namespace asio = boost::asio;

  // Terminal display of the launcher's transfers and messages.
  //
  class progress_coordinator
  {
  public:
    using manager_type = progress_manager;
    using entry_type = progress_entry;
    using metrics_type = progress_metrics;

    explicit
    progress_coordinator (asio::io_context& ioc);

    progress_coordinator (const progress_coordinator&) = delete;
    progress_coordinator& operator= (const progress_coordinator&) = delete;

    // Start drawing the display, which the update and render loops then
    // refresh on the io_context. Starting a running display does nothing.
    //
    void
    start ();

    // Stop the loops and restore the terminal. Stopping a stopped display
    // does nothing.
    //
    asio::awaitable<void>
    stop ();

    bool
    running () const noexcept;

    // Add a progress bar with the label and return its entry, which
    // update_progress() takes.
    //
    std::shared_ptr<entry_type>
    add_entry (std::string label);

    void
    remove_entry (std::shared_ptr<entry_type> entry);

    // Set the bytes transferred so far and the total of the entry, which
    // also updates its speed. The entry is completed once it reaches a
    // non-zero total.
    //
    void
    update_progress (std::shared_ptr<entry_type> entry,
                     std::uint64_t current_bytes,
                     std::uint64_t total_bytes);

    // Add the message to the lines shown below the progress bars (see
    // progress_manager::add_log()).
    //
    void
    add_log (std::string message);

    // Show the message in a box over the progress, which we use for what the
    // user has to notice.
    //
    void
    show_dialog (std::string title, std::string message);

    void
    hide_dialog ();

    manager_type&
    manager () noexcept;

    const manager_type&
    manager () const noexcept;

  private:
    manager_type manager_;
  };

  // Return the progress of the metrics as text, leaving out what isn't
  // known yet (e.g., "1.5 MiB / 3.0 MiB (50.0%) @ 512.0 KiB/s, ETA 3s").
  //
  std::string
  format_progress (const progress_metrics& metrics);

  // Return the size with a binary prefix (e.g., "1.5 MiB").
  //
  std::string
  format_bytes (std::uint64_t bytes);

  // Return the speed with a binary prefix (e.g., "2.5 MiB/s").
  //
  std::string
  format_speed (float bytes_per_sec);

  // Return the duration in its two largest units, leaving out a zero second
  // unit (e.g., "45s", "1m 30s", "2h").
  //
  std::string
  format_duration (int seconds);

  // Return the progress bar of the ratio, with the width between the
  // brackets (e.g., "[===>    ]"). The indeterminate bar has its marker in
  // the middle.
  //
  std::string
  format_progress_bar (float progress_ratio,
                       int width = 40,
                       bool indeterminate = false);
}
