#pragma once

#include <launcher/progress/progress-types.hxx>
#include <launcher/progress/progress-tracker.hxx>
#include <launcher/progress/progress-renderer.hxx>

#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace launcher
{
  namespace asio = boost::asio;

  // Manager traits for customization.
  //
  template <typename S = std::string>
  struct progress_manager_traits
  {
    using string_type = S;
    using executor_type = asio::any_io_executor;

    // Update interval in milliseconds.
    //
    static constexpr int update_interval_ms = 100;

    // Render interval in milliseconds.
    //
    static constexpr int render_interval_ms = 50;
  };

  // Managed progress item (lock-free).
  //
  template <typename T = progress_manager_traits<>>
  class basic_progress_entry
  {
  public:
    using traits_type = T;
    using string_type = typename traits_type::string_type;
    using tracker_type =
      basic_progress_tracker<progress_tracker_traits<string_type>>;

    basic_progress_entry (string_type label)
      : label_ (std::move (label))
    {
    }

    // Get label.
    //
    const string_type&
    label () const noexcept
    {
      return label_;
    }

    // Get metrics (lock-free).
    //
    progress_metrics&
    metrics () noexcept
    {
      return metrics_;
    }

    const progress_metrics&
    metrics () const noexcept
    {
      return metrics_;
    }

    // Get tracker (for speed calculations).
    //
    tracker_type&
    tracker () noexcept
    {
      return tracker_;
    }

    const tracker_type&
    tracker () const noexcept
    {
      return tracker_;
    }

    // Create snapshot (lock-free read).
    //
    progress_snapshot
    snapshot () const
    {
      return progress_snapshot (metrics_);
    }

  private:
    string_type label_;
    progress_metrics metrics_;
    tracker_type tracker_;
  };

  // Async progress manager using Boost.ASIO (lock-free, non-blocking).
  //
  template <typename T = progress_manager_traits<>>
  class basic_progress_manager
  {
  public:
    using traits_type = T;
    using string_type = typename traits_type::string_type;
    using executor_type = typename traits_type::executor_type;
    using entry_type = basic_progress_entry<traits_type>;
    using renderer_type =
      basic_progress_renderer<progress_renderer_traits<string_type>>;
    using context_type = basic_progress_render_context<string_type>;

    explicit
    basic_progress_manager (asio::io_context& ioc);

    ~basic_progress_manager ();

    // Non-copyable, non-movable.
    //
    basic_progress_manager (const basic_progress_manager&) = delete;
    basic_progress_manager& operator= (const basic_progress_manager&) = delete;

    basic_progress_manager (basic_progress_manager&&) = delete;
    basic_progress_manager& operator= (basic_progress_manager&&) = delete;

    // Start manager (non-blocking, starts async operations).
    //
    void
    start ();

    // Stop manager (stops async operations, waits for completion).
    //
    asio::awaitable<void>
    stop ();

    // Add progress entry (returns shared pointer for updates).
    //
    std::shared_ptr<entry_type>
    add_entry (string_type label);

    // Remove progress entry.
    //
    void
    remove_entry (std::shared_ptr<entry_type> entry);

    // Add log message.
    //
    void
    add_log (string_type message);

    // Get IO context.
    //
    asio::io_context&
    io_context () noexcept
    {
      return ioc_;
    }

    // Check if running.
    //
    bool
    running () const noexcept
    {
      return running_.load (std::memory_order_relaxed);
    }

  private:
    // Async update loop (coroutine).
    //
    asio::awaitable<void>
    update_loop ();

    // Async render loop (coroutine).
    //
    asio::awaitable<void>
    render_loop ();

    // Collect render context (lock-free).
    //
    context_type
    collect_context ();

    asio::io_context& ioc_;
    asio::steady_timer update_timer_;
    asio::steady_timer render_timer_;

    renderer_type renderer_;

    std::atomic<bool> running_ {false};

    // Entries (double-buffered for lock-free render).
    //
    asio::strand<executor_type> strand_;
    std::vector<std::shared_ptr<entry_type>> entries_buffers_[2];
    std::atomic<int> entries_buffer_ {0};
    bool entries_dirty_ {false};

    // Overall metrics (lock-free).
    //
    progress_metrics overall_metrics_;

    // Overall tracker (for speed calculations).
    //
    basic_progress_tracker<progress_tracker_traits<string_type>> overall_tracker_;

    // Cumulative bytes from completed/removed items.
    //
    std::atomic<std::uint64_t> cumulative_completed_bytes_ {0};
    std::atomic<std::uint64_t> cumulative_total_bytes_ {0};

    // Log messages (double-buffered, lock-free read).
    //
    std::vector<string_type> log_buffers_[2];
    std::atomic<int> log_buffer_ {0};
  };

  using progress_entry = basic_progress_entry<>;
  using progress_manager = basic_progress_manager<>;
}

#include <launcher/progress/progress-manager.txx>
