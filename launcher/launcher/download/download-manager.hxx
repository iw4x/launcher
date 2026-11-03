// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>
#include <memory>
#include <queue>
#include <functional>
#include <cstddef>

#include <boost/asio.hpp>

#include <launcher/http/http-client.hxx>
#include <launcher/download/download-task.hxx>
#include <launcher/download/download-types.hxx>

namespace launcher
{
  // Queue of download tasks performed on the io_context, at most
  // max_parallel at a time and in the order of their priority.
  //
  class download_manager
  {
  public:
    using executor_type = boost::asio::io_context::executor_type;

    using completion_callback =
      std::function<void (std::shared_ptr<launcher::download_task>)>;

    using batch_completion_callback =
      std::function<void (std::size_t completed, std::size_t failed)>;

  public:
    explicit
    download_manager (boost::asio::io_context& ioc,
                      std::size_t max_parallel = 1);

    download_manager (boost::asio::io_context& ioc,
                      std::size_t max_parallel,
                      const http_client_traits& traits);

    download_manager (const download_manager&) = delete;
    download_manager& operator= (const download_manager&) = delete;

    void
    set_max_parallel (std::size_t n);

    std::size_t
    max_parallel () const;

    // Tasks.
    //
  public:
    std::shared_ptr<launcher::download_task>
    add_task (download_request req);

    std::shared_ptr<launcher::download_task>
    add_task (download_request req, default_download_handler hdl);

    void
    add_task (std::shared_ptr<launcher::download_task> task);

    const std::vector<std::shared_ptr<launcher::download_task>>&
    tasks () const;

    std::vector<std::shared_ptr<launcher::download_task>>&
    tasks ();

    void
    clear ();

    // Progress.
    //
  public:
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

    // Download.
    //
  public:
    // Set the callback that download_all() calls for each task once it is
    // finished, successfully or not.
    //
    void
    set_task_completion_callback (completion_callback cb);

    // Set the callback that download_all() calls once all the tasks are
    // finished.
    //
    void
    set_batch_completion_callback (batch_completion_callback cb);

    // Download all the tasks that are not yet finished, in the order of
    // their priority. Note that a failed download doesn't fail the batch: it
    // is reflected in the task and in the counts.
    //
    boost::asio::awaitable<void>
    download_all ();

    // Download the task, trying its URLs in order, and set its error if the
    // download fails.
    //
    boost::asio::awaitable<void>
    download_task (std::shared_ptr<launcher::download_task> task);

    // Request every task to cancel, pause, or resume (see download_task for
    // details).
    //
    void
    cancel_all ();

    void
    pause_all ();

    void
    resume_all ();

  private:
    boost::asio::io_context& ioc_;
    std::size_t max_parallel_;
    http_client_traits traits_;
    std::vector<std::shared_ptr<launcher::download_task>> tasks_;

    completion_callback on_task_complete_;
    batch_completion_callback on_batch_complete_;

    // Return the tasks ordered by priority, highest first.
    //
    std::vector<std::shared_ptr<launcher::download_task>>
    sort_by_priority () const;
  };
}
