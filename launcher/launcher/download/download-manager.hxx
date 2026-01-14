#pragma once

#include <vector>
#include <memory>
#include <queue>
#include <functional>
#include <cstddef>

#include <boost/asio.hpp>

#include <launcher/download/download-task.hxx>
#include <launcher/download/download-types.hxx>

namespace launcher
{
  // Forward declarations.
  //
  template <typename H, typename T> class basic_download_manager;

  // Download manager traits.
  //
  template <typename H, typename S = H>
  struct download_manager_traits
  {
    using handler_type = H;
    using string_type = S;

    using task_traits = download_task_traits<handler_type, string_type>;
    using task_type = basic_download_task<handler_type, task_traits>;
    using request_type = typename task_type::request_type;

    // Executor/executor type for async operations.
    //
    using executor_type = boost::asio::io_context::executor_type;

    // Task completion callback.
    //
    using completion_callback =
      std::function<void (std::shared_ptr<task_type>)>;

    // Batch completion callback.
    //
    using batch_completion_callback =
      std::function<void (std::size_t completed, std::size_t failed)>;
  };

  // Basic download manager.
  //
  template <typename H, typename T = download_manager_traits<H>>
  class basic_download_manager
  {
  public:
    using traits_type = T;
    using handler_type = typename traits_type::handler_type;
    using task_type = typename traits_type::task_type;
    using request_type = typename traits_type::request_type;
    using completion_callback = typename traits_type::completion_callback;
    using batch_completion_callback = typename traits_type::batch_completion_callback;

    // Constructors.
    //
    explicit
    basic_download_manager (boost::asio::io_context& ioc,
                            std::size_t max_parallel = 1)
      : ioc_ (ioc),
        max_parallel_ (max_parallel)
    {
    }

    basic_download_manager (const basic_download_manager&) = delete;
    basic_download_manager& operator= (const basic_download_manager&) = delete;

    // Configuration.
    //
    void
    set_max_parallel (std::size_t n)
    {
      max_parallel_ = n;
    }

    std::size_t
    max_parallel () const
    {
      return max_parallel_;
    }

    // Task management.
    //
    std::shared_ptr<task_type>
    add_task (request_type req)
    {
      auto task (std::make_shared<task_type> (std::move (req)));
      tasks_.push_back (task);
      return task;
    }

    std::shared_ptr<task_type>
    add_task (request_type req, handler_type hdl)
    {
      auto task (std::make_shared<task_type> (std::move (req), std::move (hdl)));
      tasks_.push_back (task);
      return task;
    }

    void
    add_task (std::shared_ptr<task_type> task)
    {
      tasks_.push_back (std::move (task));
    }

    // Access tasks.
    //
    const std::vector<std::shared_ptr<task_type>>&
    tasks () const
    {
      return tasks_;
    }

    std::vector<std::shared_ptr<task_type>>&
    tasks ()
    {
      return tasks_;
    }

    // Progress and statistics.
    //
    std::size_t
    total_count () const
    {
      return tasks_.size ();
    }

    std::size_t
    completed_count () const
    {
      std::size_t count (0);
      for (const auto& task : tasks_)
        if (task->completed ())
          ++count;
      return count;
    }

    std::size_t
    failed_count () const
    {
      std::size_t count (0);
      for (const auto& task : tasks_)
        if (task->failed ())
          ++count;
      return count;
    }

    std::size_t
    active_count () const
    {
      std::size_t count (0);
      for (const auto& task : tasks_)
        if (task->active ())
          ++count;
      return count;
    }

    std::uint64_t
    total_bytes () const
    {
      std::uint64_t total (0);
      for (const auto& task : tasks_)
        total += task->total_bytes.load ();
      return total;
    }

    std::uint64_t
    downloaded_bytes () const
    {
      std::uint64_t total (0);
      for (const auto& task : tasks_)
        total += task->downloaded_bytes.load ();
      return total;
    }

    download_progress
    overall_progress () const
    {
      return download_progress (total_bytes (), downloaded_bytes ());
    }

    // Callbacks.
    //
    void
    set_task_completion_callback (completion_callback cb)
    {
      on_task_complete_ = std::move (cb);
    }

    void
    set_batch_completion_callback (batch_completion_callback cb)
    {
      on_batch_complete_ = std::move (cb);
    }

    // Download operations (coroutine-based).
    //
    boost::asio::awaitable<void>
    download_all ();

    boost::asio::awaitable<void>
    download_task (std::shared_ptr<task_type> task);

    // Control operations.
    //
    void
    cancel_all ()
    {
      for (auto& task : tasks_)
        task->cancel ();
    }

    void
    pause_all ()
    {
      for (auto& task : tasks_)
        task->pause ();
    }

    void
    resume_all ()
    {
      for (auto& task : tasks_)
        task->resume ();
    }

    void
    clear ()
    {
      tasks_.clear ();
    }

  private:
    boost::asio::io_context& ioc_;
    std::size_t max_parallel_;
    std::vector<std::shared_ptr<task_type>> tasks_;

    completion_callback on_task_complete_;
    batch_completion_callback on_batch_complete_;

    // Helper: Sort tasks by priority.
    //
    std::vector<std::shared_ptr<task_type>>
    sort_by_priority () const;
  };

  // Default manager type.
  //
  using download_manager =
    basic_download_manager<default_download_handler,
                           download_manager_traits<default_download_handler,
                                                   std::string>>;
}

#include <launcher/download/download-manager.ixx>
#include <launcher/download/download-manager.txx>
