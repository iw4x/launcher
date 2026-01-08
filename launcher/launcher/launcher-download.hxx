#pragma once

#include <launcher/download/download.hxx>
#include <launcher/http/http.hxx>

#include <boost/asio.hpp>

#include <memory>
#include <functional>
#include <filesystem>
#include <cstdint>

namespace launcher
{
  namespace fs = std::filesystem;
  namespace asio = boost::asio;

  class download_coordinator
  {
  public:
    using manager_type = download_manager;
    using task_type = typename manager_type::task_type;
    using request_type = download_request;
    using response_type = download_response;

    // Task completion callback.
    //
    // Called when a task completes (success or failure).
    //
    using completion_callback = std::function<void (std::shared_ptr<task_type>)>;

    // Batch completion callback.
    //
    // Called when all queued tasks have finished.
    //
    using batch_completion_callback =
      std::function<void (std::size_t completed, std::size_t failed)>;

    // Constructors.
    //
    explicit
    download_coordinator (asio::io_context& ioc);

    download_coordinator (asio::io_context& ioc,
                          std::size_t max_parallel);

    download_coordinator (const download_coordinator&) = delete;
    download_coordinator& operator= (const download_coordinator&) = delete;

    // Configuration.
    //
    void
    set_max_parallel (std::size_t n);

    std::size_t
    max_parallel () const;

    void
    set_completion_callback (completion_callback cb);

    void
    set_batch_completion_callback (batch_completion_callback cb);

    // Task queuing.
    //
    // Queue a download task with explicit request details. The task will be
    // added to the manager's queue but not started until execute_all() is
    // called.
    //
    std::shared_ptr<task_type>
    queue_download (request_type req);

    // Convenience: queue a simple URL-to-file download.
    //
    std::shared_ptr<task_type>
    queue_download (std::string url, fs::path target);

    // Convenience: queue a download with verification.
    //
    std::shared_ptr<task_type>
    queue_download (std::string url,
                    fs::path target,
                    download_verification verification_method,
                    std::string verification_value);

    // Statistics.
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

    // Task access.
    //
    // Get all queued tasks.
    //
    std::vector<std::shared_ptr<task_type>>
    tasks () const;

    // Execution.
    //
    // Execute all queued tasks in parallel (respecting max_parallel).
    //
    asio::awaitable<void>
    execute_all ();

    // Clear all tasks.
    //
    void
    clear ();

    // Access underlying manager.
    //
    manager_type&
    manager () noexcept;

    const manager_type&
    manager () const noexcept;

  private:
    asio::io_context& ioc_;
    std::unique_ptr<manager_type> manager_;
    std::unique_ptr<http_client> http_;
  };
}
