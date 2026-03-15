#include <launcher/download/download-manager.hxx>

#include <algorithm>
#include <fstream>
#include <chrono>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/steady_timer.hpp>

#include <launcher/http/http-client.hxx>

namespace launcher
{
  download_manager::download_manager (boost::asio::io_context& ioc,
                                      std::size_t max_parallel)
    : ioc_ (ioc),
      max_parallel_ (max_parallel)
  {
  }

  void
  download_manager::set_max_parallel (std::size_t n)
  {
    max_parallel_ = n;
  }

  std::size_t
  download_manager::max_parallel () const
  {
    return max_parallel_;
  }

  std::shared_ptr<launcher::download_task>
  download_manager::add_task (download_request req)
  {
    auto task (std::make_shared<launcher::download_task> (std::move (req)));
    tasks_.push_back (task);
    return task;
  }

  std::shared_ptr<launcher::download_task>
  download_manager::add_task (download_request req, default_download_handler hdl)
  {
    auto task (std::make_shared<launcher::download_task> (std::move (req), std::move (hdl)));
    tasks_.push_back (task);
    return task;
  }

  void
  download_manager::add_task (std::shared_ptr<launcher::download_task> task)
  {
    tasks_.push_back (std::move (task));
  }

  const std::vector<std::shared_ptr<launcher::download_task>>&
  download_manager::tasks () const
  {
    return tasks_;
  }

  std::vector<std::shared_ptr<launcher::download_task>>&
  download_manager::tasks ()
  {
    return tasks_;
  }

  std::size_t
  download_manager::total_count () const
  {
    return tasks_.size ();
  }

  std::size_t
  download_manager::completed_count () const
  {
    std::size_t count (0);
    for (const auto& task : tasks_)
      if (task->completed ())
        ++count;
    return count;
  }

  std::size_t
  download_manager::failed_count () const
  {
    std::size_t count (0);
    for (const auto& task : tasks_)
      if (task->failed ())
        ++count;
    return count;
  }

  std::size_t
  download_manager::active_count () const
  {
    std::size_t count (0);
    for (const auto& task : tasks_)
      if (task->active ())
        ++count;
    return count;
  }

  std::uint64_t
  download_manager::total_bytes () const
  {
    std::uint64_t total (0);
    for (const auto& task : tasks_)
      total += task->total_bytes.load ();
    return total;
  }

  std::uint64_t
  download_manager::downloaded_bytes () const
  {
    std::uint64_t total (0);
    for (const auto& task : tasks_)
      total += task->downloaded_bytes.load ();
    return total;
  }

  download_progress
  download_manager::overall_progress () const
  {
    return download_progress (total_bytes (), downloaded_bytes ());
  }

  void
  download_manager::set_task_completion_callback (completion_callback cb)
  {
    on_task_complete_ = std::move (cb);
  }

  void
  download_manager::set_batch_completion_callback (batch_completion_callback cb)
  {
    on_batch_complete_ = std::move (cb);
  }

  void
  download_manager::cancel_all ()
  {
    for (auto& task : tasks_)
      task->cancel ();
  }

  void
  download_manager::pause_all ()
  {
    for (auto& task : tasks_)
      task->pause ();
  }

  void
  download_manager::resume_all ()
  {
    for (auto& task : tasks_)
      task->resume ();
  }

  void
  download_manager::clear ()
  {
    tasks_.clear ();
  }

  std::vector<std::shared_ptr<launcher::download_task>>
  download_manager::sort_by_priority () const
  {
    std::vector<std::shared_ptr<launcher::download_task>> r (tasks_);

    std::stable_sort (r.begin (), r.end (), [] (const auto& a, const auto& b)
    {
      return a->request.priority > b->request.priority;
    });

    return r;
  }

  boost::asio::awaitable<void>
  download_manager::download_all ()
  {
    if (tasks_.empty ())
      co_return;

    auto sorted_tasks (sort_by_priority ());

    std::vector<std::shared_ptr<launcher::download_task>> active_tasks;
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
          this->download_task (task),
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
            this->download_task (task),
            boost::asio::detached);
      }

      boost::asio::steady_timer timer (ioc_, std::chrono::milliseconds (50));
      co_await timer.async_wait (boost::asio::use_awaitable);
    }

    if (on_batch_complete_)
      on_batch_complete_ (completed_count (), failed_count ());
  }

  boost::asio::awaitable<void>
  download_manager::download_task (std::shared_ptr<launcher::download_task> task)
  {
    if (!task->request.valid ())
    {
      task->set_error (download_error ("Invalid download request"));
      co_return;
    }

    task->response.start_time = std::chrono::steady_clock::now ();

    http_client_traits traits;
    traits.connect_timeout = task->request.connect_timeout * 1000;
    traits.request_timeout = task->request.transfer_timeout * 1000;
    traits.follow_redirects = true;

    http_client client (ioc_, traits);

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

          task->update_progress (transferred, total);
          task->response.progress.speed_bps = 0; // calculated by caller/ui
        });

        std::uint64_t bytes_downloaded (
          co_await client.download (url,
                                    task->request.target.string (),
                                    progress_callback,
                                    resume_from,
                                    task->request.rate_limit_bytes_per_second));

        task->update_progress (bytes_downloaded, bytes_downloaded);
        task->response.http_status_code = 200;
        task->response.server_reported_size = bytes_downloaded;

        task->response.successful_url_index = i;
        task->set_state (download_state::completed);
        success = true;
        break;
      }
      catch (const std::exception& e)
      {
        std::string s (e.what ());

        if (s.find ("416") != std::string::npos && resume_from)
        {
          std::error_code ec;
          fs::remove (task->request.target, ec);
          resume_from = std::nullopt;

          --i;
          continue;
        }

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
