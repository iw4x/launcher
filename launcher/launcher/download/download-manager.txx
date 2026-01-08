#include <algorithm>
#include <fstream>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/steady_timer.hpp>

#include <launcher/http/http-client.hxx>

namespace launcher
{
  // Execute all queued tasks respecting the concurrency limit.
  //
  template <typename H, typename T>
  boost::asio::awaitable<void> basic_download_manager<H, T>::
  download_all ()
  {
    if (tasks_.empty ())
      co_return;

    // Sort tasks so that high-priority items (e.g., base game files) are
    // downloaded before optional content or lower-priority assets.
    //
    auto sorted_tasks (sort_by_priority ());

    std::vector<std::shared_ptr<task_type>> active_tasks;
    std::size_t next_task_index (0);

    // Start the initial batch of downloads up to the concurrency limit.
    //
    while (next_task_index < sorted_tasks.size () &&
           active_tasks.size () < max_parallel_)
    {
      auto task (sorted_tasks[next_task_index++]);

      // Skip tasks that are already in a terminal state (cached or failed
      // previously).
      //
      if (task->completed () || task->failed ())
        continue;

      active_tasks.push_back (task);

      boost::asio::co_spawn (
          ioc_,
          download_task (task),
          boost::asio::detached);
    }

    // Main event loop: wait for tasks to complete and replenish the queue.
    //
    while (!active_tasks.empty () || next_task_index < sorted_tasks.size ())
    {
      // Remove completed or failed tasks from the active list.
      //
      // We also trigger the per-task completion callback here to notify the UI
      // or caller of incremental progress.
      //
      active_tasks.erase (std::remove_if (active_tasks.begin (),
                                          active_tasks.end (),
                                          [this] (const auto& t)
      {
        bool done (t->completed () || t->failed ());

        if (done && on_task_complete_)
          on_task_complete_ (t);

        return done;
      }), active_tasks.end ());

      // Replenish the active queue.
      //
      // If we have capacity and pending tasks, spawn new coroutines
      // immediately.
      //
      while (next_task_index < sorted_tasks.size () &&
             active_tasks.size () < max_parallel_)
      {
        auto task (sorted_tasks[next_task_index++]);

        if (task->completed () || task->failed ())
          continue;

        active_tasks.push_back (task);

        boost::asio::co_spawn (
            ioc_,
            download_task (task),
            boost::asio::detached);
      }

      // Throttle the loop.
      //
      // Since we don't have a direct "wait for any coroutine" signal here
      // without more complex logic (like a channel or condition variable), we
      // sleep briefly to avoid busy-waiting while polling the task states.
      //
      boost::asio::steady_timer timer (ioc_, std::chrono::milliseconds (50));
      co_await timer.async_wait (boost::asio::use_awaitable);
    }

    if (on_batch_complete_)
      on_batch_complete_ (completed_count (), failed_count ());
  }

  // Download a single task.
  //
  template <typename H, typename T>
  boost::asio::awaitable<void> basic_download_manager<H, T>::
  download_task (std::shared_ptr<task_type> task)
  {
    if (!task->request.valid ())
    {
      task->set_error (download_error ("Invalid download request"));
      co_return;
    }

    task->response.start_time = std::chrono::steady_clock::now ();

    // Configure the HTTP client.
    //
    // We convert the timeout values from seconds (user config) to
    // milliseconds.
    //
    http_client_traits<> traits;
    traits.connect_timeout = task->request.connect_timeout * 1000;
    traits.request_timeout = task->request.transfer_timeout * 1000;
    traits.follow_redirects = true;

    basic_http_client<> client (ioc_, traits);

    // Check for an existing file to resume from.
    //
    // If the file exists and has content, we assume it is a partial download
    // corresponding to the beginning of the file.
    //
    std::optional<std::uint64_t> resume_from;
    if (task->request.resume && fs::exists (task->request.target))
    {
      std::error_code ec;
      std::uint64_t existing_size (fs::file_size (task->request.target, ec));

      if (!ec && existing_size > 0)
      {
        resume_from = existing_size;
        task->update_progress (existing_size,
                              task->request.expected_size.value_or (0));
      }
    }

    // Iterate over mirrors and attempt to download.
    //
    bool success (false);
    for (std::size_t i (0); i < task->request.urls.size (); ++i)
    {
      if (task->should_cancel ())
      {
        task->set_error (download_error ("Download cancelled"));
        break;
      }

      // Handle pause requests.
      //
      // If the task is paused, we suspend the coroutine until the state
      // changes, checking periodically.
      //
      while (task->should_pause ())
      {
        task->set_state (download_state::paused);
        boost::asio::steady_timer timer (ioc_, std::chrono::milliseconds (100));
        co_await timer.async_wait (boost::asio::use_awaitable);
      }

      const auto& url (task->request.urls[i]);

      try
      {
        task->set_state (download_state::connecting);
        task->set_state (download_state::downloading);

        // Define the progress callback.
        //
        // Note: The client.download call drives this callback. While we can
        // check for cancellation here, pausing is harder to handle
        // mid-transfer without dropping the connection.
        //
        auto progress_callback ([task, this](std::uint64_t transferred,
                                              std::uint64_t total)
        {
          if (task->should_cancel ())
            throw std::runtime_error ("Download cancelled");

          task->update_progress (transferred, total);
          task->response.progress.speed_bps = 0; // calculated by caller/ui
        });

        std::uint64_t bytes_downloaded (
          co_await client.download (url,
                                    task->request.target.string (),
                                    progress_callback,
                                    resume_from));

        task->update_progress (bytes_downloaded, bytes_downloaded);
        task->response.http_status_code = 200; // Successful download
        task->response.server_reported_size = bytes_downloaded;

        // Verify the downloaded file.
        //
        if (task->request.verification_method != download_verification::none &&
            !task->request.verification_value.empty ())
        {
          task->set_state (download_state::verifying);

          // Compute the hash before comparison so we can display the mismatch
          // in the logs if verification fails.
          //
          std::string computed_hash (
            T::task_traits::compute_hash (task->request.target,
                                          task->request.verification_method));

          bool verified (
            !computed_hash.empty () &&
            T::task_traits::compare_hashes (computed_hash,
                                            task->request.verification_value));

          task->response.verification_passed = verified;

          if (!verified)
          {
            auto& error (std::cerr);

            error << "checksum verification failed for file: "
                  << task->request.name;
            error << "expected: " << task->request.verification_value;
            error << "found:    "
                  << (computed_hash.empty () ? "(failed)" : computed_hash);
            error << "file:     " << task->request.target;
            error << "installation aborted: file may be corrupted";

            // Cleanup the corrupted file so we don't resume from it next time.
            //
            std::error_code ec;
            fs::remove (task->request.target, ec);

            task->set_error (
              download_error ("Installation aborted. File may be corrupted.",
                              url,
                              0));

            // @@ This is a library function, but we are calling exit(1). This
            // suggests a fatal error in the deployment logic (e.g., a master
            // server providing bad hashes) that we cannot recover from safely.
            //
            std::exit (1);
          }
        }
        else
          task->response.verification_passed = true;

        task->response.successful_url_index = i;
        task->set_state (download_state::completed);
        success = true;
        break;
      }
      // Handle download failures.
      //
      // @@ The current behavior retries subsequent URLs on failure. This is
      // primarily to aid debugging or handle flaky mirrors. For a strict
      // release build, we might prefer to fail fast.
      //
      catch (const std::exception& e)
      {
        if (i == task->request.urls.size () - 1)
        {
          // All mirrors failed.
          //
          task->set_error (download_error (
              std::string ("Download failed: ") + e.what (),
              url,
              0));
        }
        else
        {
          // Try the next URL.
          //
          // If we partially downloaded the file, attempt to resume from the
          // current size.
          //
          if (task->request.resume && fs::exists (task->request.target))
          {
            std::error_code ec;
            std::uint64_t existing_size (
              fs::file_size (task->request.target, ec));

            if (!ec && existing_size > 0)
              resume_from = existing_size;
          }
        }
      }
    }

    task->response.end_time = std::chrono::steady_clock::now ();
  }
}
