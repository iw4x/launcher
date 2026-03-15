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
  class download_manager
  {
  public:
    using executor_type = boost::asio::io_context::executor_type;

    using completion_callback =
      std::function<void (std::shared_ptr<launcher::download_task>)>;

    using batch_completion_callback =
      std::function<void (std::size_t completed, std::size_t failed)>;

    // Constructors.
    //
    explicit
    download_manager (boost::asio::io_context& ioc,
                      std::size_t max_parallel = 1);

    download_manager (const download_manager&) = delete;
    download_manager& operator= (const download_manager&) = delete;

    // Configuration.
    //
    void
    set_max_parallel (std::size_t n);

    std::size_t
    max_parallel () const;

    // Task management.
    //
    std::shared_ptr<launcher::download_task>
    add_task (download_request req);

    std::shared_ptr<launcher::download_task>
    add_task (download_request req, default_download_handler hdl);

    void
    add_task (std::shared_ptr<launcher::download_task> task);

    // Access tasks.
    //
    const std::vector<std::shared_ptr<launcher::download_task>>&
    tasks () const;

    std::vector<std::shared_ptr<launcher::download_task>>&
    tasks ();

    // Progress and statistics.
    //
    std::size_t
    total_count () const;

    std::size_t
    completed_count () const;

    std::size_t
    failed_count () const;

    std::size_t
    active_count () const;

    std::uint64_t
    total_bytes () const;

    std::uint64_t
    downloaded_bytes () const;

    download_progress
    overall_progress () const;

    // Callbacks.
    //
    void
    set_task_completion_callback (completion_callback cb);

    void
    set_batch_completion_callback (batch_completion_callback cb);

    // Download operations (coroutine-based).
    //
    boost::asio::awaitable<void>
    download_all ();

    boost::asio::awaitable<void>
    download_task (std::shared_ptr<launcher::download_task> task);

    // Control operations.
    //
    void
    cancel_all ();

    void
    pause_all ();

    void
    resume_all ();

    void
    clear ();

  private:
    boost::asio::io_context& ioc_;
    std::size_t max_parallel_;
    std::vector<std::shared_ptr<launcher::download_task>> tasks_;

    completion_callback on_task_complete_;
    batch_completion_callback on_batch_complete_;

    // Helper: Sort tasks by priority.
    //
    std::vector<std::shared_ptr<launcher::download_task>>
    sort_by_priority () const;
  };
}
