// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <launcher/download/download.hxx>
#include <launcher/http/http.hxx>

#include <boost/asio.hpp>

#include <functional>
#include <filesystem>
#include <cstdint>

namespace launcher
{
  namespace fs = std::filesystem;
  namespace asio = boost::asio;

  // Batch of downloads that run in parallel up to a limit, with the
  // progress of the whole batch.
  //
  class download_coordinator
  {
  public:
    using manager_type = download_manager;
    using task_type = download_task;
    using request_type = download_request;
    using response_type = download_response;

    // Called for each task once it finishes, successfully or not.
    //
    using completion_callback = std::function<void (std::shared_ptr<task_type>)>;

    // Called once all the queued tasks have finished, with the number of
    // the ones that completed and failed.
    //
    using batch_completion_callback =
      std::function<void (std::size_t completed, std::size_t failed)>;

    // By default the downloads run one at a time.
    //
    explicit
    download_coordinator (asio::io_context& ioc);

    download_coordinator (asio::io_context& ioc,
                          std::size_t max_parallel);

    download_coordinator (asio::io_context& ioc,
                          std::size_t max_parallel,
                          const http_client_traits& traits);

    download_coordinator (const download_coordinator&) = delete;
    download_coordinator& operator= (const download_coordinator&) = delete;

    void
    set_max_parallel (std::size_t n);

    std::size_t
    max_parallel () const;

    void
    set_completion_callback (completion_callback cb);

    void
    set_batch_completion_callback (batch_completion_callback cb);

    // Queue the download and return its task, which only starts with
    // execute_all().
    //
    std::shared_ptr<task_type>
    queue_download (request_type req);

    // Queue the download of the URL into the file, naming the task after the
    // file.
    //
    std::shared_ptr<task_type>
    queue_download (std::string url, fs::path target);

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

    std::vector<std::shared_ptr<task_type>>
    tasks () const;

    // Run the queued tasks, at most max_parallel() at a time, and return once
    // they have all finished.
    //
    asio::awaitable<void>
    execute_all ();

    void
    clear ();

    manager_type&
    manager () noexcept;

    const manager_type&
    manager () const noexcept;

  private:
    asio::io_context& ioc_;
    manager_type manager_;
    http_client http_;
    http_client_traits traits_;
  };
}
