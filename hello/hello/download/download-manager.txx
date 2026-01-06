#include <algorithm>
#include <fstream>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/steady_timer.hpp>

#include <hello/http/http-client.hxx>

namespace hello
{
  template <typename H, typename T>
  boost::asio::awaitable<void> basic_download_manager<H, T>::
  download_all ()
  {
    if (tasks_.empty ())
      co_return;

    auto sorted_tasks (sort_by_priority ());

    std::vector<std::shared_ptr<task_type>> active_tasks;
    std::size_t next_task_index (0);

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

    while (!active_tasks.empty () || next_task_index < sorted_tasks.size ())
    {
      active_tasks.erase (std::remove_if (active_tasks.begin (),
                                          active_tasks.end (),
                                          [this] (const auto& t)
      {
        bool done (t->completed () || t->failed ());
        if (done && on_task_complete_)
          on_task_complete_ (t);
        return done;
      }), active_tasks.end ());

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

      boost::asio::steady_timer timer (ioc_, std::chrono::milliseconds (50));
      co_await timer.async_wait (boost::asio::use_awaitable);
    }

    if (on_batch_complete_)
      on_batch_complete_ (completed_count (), failed_count ());
  }

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

    http_client_traits<> traits;
    traits.connect_timeout = task->request.connect_timeout * 1000;
    traits.request_timeout = task->request.transfer_timeout * 1000;
    traits.follow_redirects = true;

    basic_http_client<> client (ioc_, traits);

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

    bool success (false);

    for (std::size_t i (0); i < task->request.urls.size (); ++i)
    {
      if (task->should_cancel ())
      {
        task->set_error (download_error ("Download cancelled"));
        break;
      }

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

        auto progress_callback ([task, this](std::uint64_t transferred,
                                              std::uint64_t total)
        {
          if (task->should_cancel ())
            throw std::runtime_error ("Download cancelled");

          // Note: Pause handling in callback is limited; better to check
          // periodically in the main loop.

          task->update_progress (transferred, total);
          task->response.progress.speed_bps = 0; // Updated by caller if needed
        });

        std::uint64_t bytes_downloaded (
            co_await client.download (
                url,
                task->request.target.string (),
                progress_callback,
                resume_from));

        task->update_progress (bytes_downloaded, bytes_downloaded);

        task->response.http_status_code = 200; // Successful download
        task->response.server_reported_size = bytes_downloaded;

        if (task->request.verification_method != download_verification::none &&
            !task->request.verification_value.empty ())
        {
          task->set_state (download_state::verifying);

          std::string computed_hash (T::task_traits::compute_hash (
              task->request.target,
              task->request.verification_method));

          bool verified (!computed_hash.empty () &&
                        T::task_traits::compare_hashes (
                            computed_hash,
                            task->request.verification_value));

          task->response.verification_passed = verified;

          if (!verified)
          {
            std::cerr << "CHECKSUM VERIFICATION FAILED for file: " << task->request.name;
            std::cerr << "Expected: " << task->request.verification_value;
            std::cerr << "Found:    " << (computed_hash.empty () ? "(failed to compute)" : computed_hash);
            std::cerr << "File: " << task->request.target;
            std::cerr << "Installation aborted. File may be corrupted or incomplete.";
            std::cerr << "Removing corrupted file and aborting download task.";

            std::error_code ec;
            fs::remove (task->request.target, ec);

            task->set_error (download_error (
                "Checksum verification failed - installation aborted",
                url,
                0));

            std::exit (1);
          }
        }
        else
        {
          task->response.verification_passed = true;
        }

        task->response.successful_url_index = i;
        task->set_state (download_state::completed);
        success = true;
        break;
      }
      catch (const std::exception& e)
      {
        if (i == task->request.urls.size () - 1)
        {
          task->set_error (download_error (
              std::string ("Download failed: ") + e.what (),
              url,
              0));
        }
        else
        {
          if (task->request.resume && fs::exists (task->request.target))
          {
            std::error_code ec;
            std::uint64_t existing_size (fs::file_size (task->request.target, ec));
            if (!ec && existing_size > 0)
              resume_from = existing_size;
          }
        }
      }
    }

    task->response.end_time = std::chrono::steady_clock::now ();
  }
}
