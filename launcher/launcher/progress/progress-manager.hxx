#pragma once

#include <launcher/progress/progress-types.hxx>
#include <launcher/progress/progress-tracker.hxx>
#include <launcher/progress/progress-renderer.hxx>

#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <atomic>

namespace launcher
{
  namespace asio = boost::asio;

  class progress_entry
  {
  public:
    explicit
    progress_entry (std::string label)
      : label_ (std::move (label))
    {
    }

    const std::string&
    label () const noexcept
    {
      return label_;
    }

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

    progress_tracker&
    tracker () noexcept
    {
      return tracker_;
    }

    const progress_tracker&
    tracker () const noexcept
    {
      return tracker_;
    }

    progress_snapshot
    snapshot () const
    {
      return progress_snapshot (metrics_);
    }

  private:
    std::string label_;
    progress_metrics metrics_;
    progress_tracker tracker_;
  };

  class progress_manager
  {
  public:
    static constexpr std::chrono::milliseconds update_interval {100};
    static constexpr std::chrono::milliseconds render_interval {50};

    explicit
    progress_manager (asio::io_context& ioc);

    ~progress_manager ();

    progress_manager (const progress_manager&) = delete;
    progress_manager& operator= (const progress_manager&) = delete;

    progress_manager (progress_manager&&) = delete;
    progress_manager& operator= (progress_manager&&) = delete;

    void start ();
    asio::awaitable<void> stop ();

    std::shared_ptr<progress_entry>
    add_entry (std::string label);

    void
    remove_entry (std::shared_ptr<progress_entry> entry);

    void
    add_log (std::string message);

    void
    show_dialog (std::string title, std::string message);

    void
    hide_dialog ();

    asio::io_context&
    io_context () noexcept
    {
      return ioc_;
    }

    bool
    running () const noexcept
    {
      return running_.load (std::memory_order_relaxed);
    }

  private:
    asio::awaitable<void> update_loop ();
    asio::awaitable<void> render_loop ();
    progress_render_context collect_context ();

    asio::io_context& ioc_;
    asio::steady_timer update_timer_;
    asio::steady_timer render_timer_;

    progress_renderer renderer_;

    std::atomic<bool> running_ {false};

    asio::strand<asio::any_io_executor> strand_;
    std::vector<std::shared_ptr<progress_entry>> entries_buffers_[2];
    std::atomic<int> entries_buffer_ {0};
    bool entries_dirty_ {false};

    progress_metrics overall_metrics_;
    progress_tracker overall_tracker_;

    std::atomic<std::uint64_t> cumulative_completed_bytes_ {0};
    std::atomic<std::uint64_t> cumulative_total_bytes_ {0};

    std::vector<std::string> log_buffers_[2];
    std::atomic<int> log_buffer_ {0};

    std::atomic<bool> dialog_visible_ {false};
    std::string dialog_title_;
    std::string dialog_message_;
    asio::strand<asio::any_io_executor> dialog_strand_;
    unsigned frame_ {0};
  };
}
