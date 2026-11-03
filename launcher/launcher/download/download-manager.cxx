// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/download/download-manager.hxx>

#include <algorithm>
#include <fstream>
#include <chrono>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/steady_timer.hpp>

using namespace std;

namespace launcher
{
  // Interval at which download_all() checks for finished tasks and starts the
  // next ones.
  //
  constexpr chrono::milliseconds reap_interval {50};

  // Interval at which a paused download checks if it should resume.
  //
  constexpr chrono::milliseconds pause_poll_interval {100};

  download_manager::
  download_manager (boost::asio::io_context& c,
                    size_t m)
    : ioc_ (c),
      max_parallel_ (m)
  {
  }

  download_manager::
  download_manager (boost::asio::io_context& c,
                    size_t m,
                    const http_client_traits& t)
    : ioc_ (c),
      max_parallel_ (m),
      traits_ (t)
  {
  }

  void
  download_manager::set_max_parallel (size_t n)
  {
    max_parallel_ = n;
  }

  size_t
  download_manager::max_parallel () const
  {
    return max_parallel_;
  }

  shared_ptr<launcher::download_task>
  download_manager::add_task (download_request r)
  {
    auto t (make_shared<launcher::download_task> (move (r)));
    tasks_.push_back (t);
    return t;
  }

  shared_ptr<launcher::download_task>
  download_manager::add_task (download_request r, default_download_handler h)
  {
    auto t (make_shared<launcher::download_task> (move (r), move (h)));
    tasks_.push_back (t);
    return t;
  }

  void
  download_manager::add_task (shared_ptr<launcher::download_task> t)
  {
    tasks_.push_back (move (t));
  }

  const vector<shared_ptr<launcher::download_task>>&
  download_manager::tasks () const
  {
    return tasks_;
  }

  vector<shared_ptr<launcher::download_task>>&
  download_manager::tasks ()
  {
    return tasks_;
  }

  size_t
  download_manager::total_count () const
  {
    return tasks_.size ();
  }

  size_t
  download_manager::completed_count () const
  {
    size_t c (0);
    for (const auto& t : tasks_)
      if (t->completed ())
        ++c;
    return c;
  }

  size_t
  download_manager::failed_count () const
  {
    size_t c (0);
    for (const auto& t : tasks_)
      if (t->failed ())
        ++c;
    return c;
  }

  size_t
  download_manager::active_count () const
  {
    size_t c (0);
    for (const auto& t : tasks_)
      if (t->active ())
        ++c;
    return c;
  }

  uint64_t
  download_manager::total_bytes () const
  {
    uint64_t b (0);
    for (const auto& t : tasks_)
      b += t->total_bytes.load ();
    return b;
  }

  uint64_t
  download_manager::downloaded_bytes () const
  {
    uint64_t b (0);
    for (const auto& t : tasks_)
      b += t->downloaded_bytes.load ();
    return b;
  }

  download_progress
  download_manager::overall_progress () const
  {
    return download_progress (total_bytes (), downloaded_bytes ());
  }

  void
  download_manager::set_task_completion_callback (completion_callback c)
  {
    on_task_complete_ = move (c);
  }

  void
  download_manager::set_batch_completion_callback (batch_completion_callback c)
  {
    on_batch_complete_ = move (c);
  }

  void
  download_manager::cancel_all ()
  {
    for (auto& t : tasks_)
      t->cancel ();
  }

  void
  download_manager::pause_all ()
  {
    for (auto& t : tasks_)
      t->pause ();
  }

  void
  download_manager::resume_all ()
  {
    for (auto& t : tasks_)
      t->resume ();
  }

  void
  download_manager::clear ()
  {
    tasks_.clear ();
  }

  vector<shared_ptr<launcher::download_task>>
  download_manager::sort_by_priority () const
  {
    vector<shared_ptr<launcher::download_task>> r (tasks_);

    // Keep the tasks of the same priority in the order that they were added.
    //
    stable_sort (r.begin (), r.end (), [] (const auto& a, const auto& b)
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

    auto s (sort_by_priority ());

    vector<shared_ptr<launcher::download_task>> a;
    size_t i (0);

    // Start the first max_parallel tasks, skipping the finished ones.
    //
    while (i < s.size () && a.size () < max_parallel_)
    {
      auto t (s[i++]);

      if (t->completed () || t->failed ())
        continue;

      a.push_back (t);

      boost::asio::co_spawn (
          ioc_,
          this->download_task (t),
          boost::asio::detached);
    }

    // Keep starting tasks as the active ones finish until nothing is left.
    //
    // @@ We poll since the tasks don't notify us when they finish.
    //
    while (!a.empty () || i < s.size ())
    {
      // Drop the finished tasks from the active set.
      //
      a.erase (remove_if (a.begin (), a.end (), [this] (const auto& t)
      {
        bool d (t->completed () || t->failed ());

        if (d && on_task_complete_)
          on_task_complete_ (t);

        return d;
      }), a.end ());

      // As above.
      //
      while (i < s.size () && a.size () < max_parallel_)
      {
        auto t (s[i++]);

        if (t->completed () || t->failed ())
          continue;

        a.push_back (t);

        boost::asio::co_spawn (
            ioc_,
            this->download_task (t),
            boost::asio::detached);
      }

      boost::asio::steady_timer tm (ioc_, reap_interval);
      co_await tm.async_wait (boost::asio::use_awaitable);
    }

    if (on_batch_complete_)
      on_batch_complete_ (completed_count (), failed_count ());
  }

  boost::asio::awaitable<void>
  download_manager::download_task (shared_ptr<launcher::download_task> t)
  {
    if (!t->request.valid ())
    {
      t->set_error (download_error ("Invalid download request"));
      co_return;
    }

    t->response.start_time = chrono::steady_clock::now ();

    // Override the client timeouts with the request ones.
    //
    http_client_traits tr (traits_);
    tr.connect_timeout = chrono::duration_cast<chrono::milliseconds> (t->request.connect_timeout);
    tr.request_timeout = chrono::duration_cast<chrono::milliseconds> (t->request.transfer_timeout);
    tr.follow_redirects = true;

    http_client c (ioc_, tr);

    optional<uint64_t> r;

    // See if there is a partial target that we can resume.
    //
    if (t->request.resume && fs::exists (t->request.target))
    {
      error_code ec;
      uint64_t s (fs::file_size (t->request.target, ec));

      if (!ec && s > 0)
      {
        r = s;
        t->update_progress (s, t->request.expected_size.value_or (0));
      }
    }

    for (size_t i (0); i < t->request.urls.size (); ++i)
    {
      if (t->should_cancel ())
      {
        t->set_error (download_error ("Download cancelled"));
        break;
      }

      // Wait while paused. Note that we only pause between attempts.
      //
      while (t->should_pause ())
      {
        t->set_state (download_state::paused);
        boost::asio::steady_timer tm (ioc_, pause_poll_interval);
        co_await tm.async_wait (boost::asio::use_awaitable);
      }

      const auto& u (t->request.urls[i]);

      try
      {
        t->set_state (download_state::connecting);
        t->set_state (download_state::downloading);

        auto cb ([t] (uint64_t tx, uint64_t tot)
        {
          if (t->should_cancel ())
            throw runtime_error ("Download cancelled");

          t->update_progress (tx, tot);
          t->response.progress.speed_bps = 0;
        });

        uint64_t b (
          co_await c.download (u,
                               t->request.target,
                               cb,
                               r,
                               t->request.rate_limit_bytes_per_second));

        t->update_progress (b, b);
        t->response.http_status_code = 200;
        t->response.server_reported_size = b;

        t->response.successful_url_index = i;
        t->set_state (download_state::completed);
        break;
      }
      catch (const http_status_error& e)
      {
        // If the server can't satisfy the range that we resume from, then
        // the partial target is either corrupt or already complete. Either
        // way, remove it and download from this URL again from scratch.
        //
        if (e.status () == http_status::range_not_satisfiable && r)
        {
          error_code ec;
          fs::remove (t->request.target, ec);
          r = nullopt;

          --i; // Retry the same URL.
          continue;
        }

        string m (e.what ());

        if (i == t->request.urls.size () - 1)
        {
          t->set_error (download_error (
              string ("Download failed: ") + m,
              u,
              0));
        }
        else
        {
          if (t->request.resume && fs::exists (t->request.target))
          {
            error_code ec;
            uint64_t s (fs::file_size (t->request.target, ec));

            if (!ec && s > 0)
              r = s;
          }
        }
      }
      catch (const exception& e)
      {
        string m (e.what ());

        // Fail if this was the last URL.
        //
        if (i == t->request.urls.size () - 1)
        {
          t->set_error (download_error (
              string ("Download failed: ") + m,
              u,
              0));
        }
        else
        {
          // Otherwise, resume from whatever the failed attempt managed to
          // write.
          //
          if (t->request.resume && fs::exists (t->request.target))
          {
            error_code ec;
            uint64_t s (fs::file_size (t->request.target, ec));

            if (!ec && s > 0)
              r = s;
          }
        }
      }
    }

    t->response.end_time = chrono::steady_clock::now ();
  }
}
