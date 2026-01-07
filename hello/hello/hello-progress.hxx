#pragma once

#include <hello/progress/progress.hxx>

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace hello
{
  namespace asio = boost::asio;

  class progress_coordinator
  {
  public:
    using manager_type = progress_manager;
    using entry_type = progress_entry;
    using metrics_type = progress_metrics;

    // Constructors.
    //
    explicit
    progress_coordinator (asio::io_context& ioc);

    progress_coordinator (const progress_coordinator&) = delete;
    progress_coordinator& operator= (const progress_coordinator&) = delete;

    // Start progress reporting.
    //
    // Begins async rendering loop. Must be called before adding entries or
    // updating progress.
    //
    void
    start ();

    // Stop progress reporting.
    //
    // Stops async rendering loop and waits for completion.
    //
    asio::awaitable<void>
    stop ();

    // Check if running.
    //
    bool
    running () const noexcept;

    // Add progress entry.
    //
    // Creates a new progress tracker with the given label. The returned
    // shared pointer must be retained to update progress.
    //
    std::shared_ptr<entry_type>
    add_entry (std::string label);

    // Remove progress entry.
    //
    void
    remove_entry (std::shared_ptr<entry_type> entry);

    // Update progress for an entry.
    //
    // Sets the current and total bytes for the given entry. The progress
    // subsystem automatically calculates speed and ETA.
    //
    void
    update_progress (std::shared_ptr<entry_type> entry,
                     std::uint64_t current_bytes,
                     std::uint64_t total_bytes);

    // Add log message.
    //
    // Appends a message to the log buffer, which is displayed below the
    // progress bars.
    //
    void
    add_log (std::string message);

    // Access underlying manager.
    //
    manager_type&
    manager () noexcept;

    const manager_type&
    manager () const noexcept;

  private:
    asio::io_context& ioc_;
    std::unique_ptr<manager_type> manager_;
  };

  // Format progress for display.
  //
  // Utility function to format progress metrics as a human-readable string.
  //
  std::string
  format_progress (const progress_metrics& metrics);

  // Format bytes as human-readable string (e.g., "1.5 MB").
  //
  std::string
  format_bytes (std::uint64_t bytes);

  // Format speed as human-readable string (e.g., "2.5 MB/s").
  //
  std::string
  format_speed (float bytes_per_sec);

  // Format duration as human-readable string (e.g., "1m 30s").
  //
  std::string
  format_duration (int seconds);

  // Format progress bar.
  //
  // Returns a string representation of a progress bar with the given width.
  //
  std::string
  format_progress_bar (float progress_ratio,
                       int width = 40,
                       bool indeterminate = false);
}
