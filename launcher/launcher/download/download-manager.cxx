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

    // Keep the insertion order stable for tasks that share the same priority.
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

    // Seed the initial batch of active tasks. We strictly adhere to our
    // concurrency limit to avoid swamping the io_context right out of the gate.
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

    // Keep the queue hot until we run out of things to process.
    //
    while (!a.empty () || i < s.size ())
    {
      // Reap completed or failed tasks from our tracking vector.
      //
      a.erase (remove_if (a.begin (), a.end (), [this] (const auto& t)
      {
        bool d (t->completed () || t->failed ());

        if (d && on_task_complete_)
          on_task_complete_ (t);

        return d;
      }), a.end ());

      // Top up the active list up to our allowed parallelism.
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

      // Yield back to the event loop so we aren't pointlessly spinning the CPU.
      //
      boost::asio::steady_timer tm (ioc_, chrono::milliseconds (50));
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

    // The client traits expect our timeouts in milliseconds.
    //
    http_client_traits tr (traits_);
    tr.connect_timeout = t->request.connect_timeout * 1000;
    tr.request_timeout = t->request.transfer_timeout * 1000;
    tr.follow_redirects = true;

    http_client c (ioc_, tr);

    optional<uint64_t> r;

    // Check if we have a partial payload lying around on disk that we can
    // resume from.
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

    bool ok (false);
    for (size_t i (0); i < t->request.urls.size (); ++i)
    {
      if (t->should_cancel ())
      {
        t->set_error (download_error ("Download cancelled"));
        break;
      }

      // Busy-wait if paused. The 100ms interval is arbitrary but responsive
      // enough without being a drag on the reactor.
      //
      while (t->should_pause ())
      {
        t->set_state (download_state::paused);
        boost::asio::steady_timer tm (ioc_, chrono::milliseconds (100));
        co_await tm.async_wait (boost::asio::use_awaitable);
      }

      const auto& u (t->request.urls[i]);

      try
      {
        t->set_state (download_state::connecting);
        t->set_state (download_state::downloading);

        auto cb ([t, this] (uint64_t tx, uint64_t tot)
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
        ok = true;
        break;
      }
      catch (const exception& e)
      {
        string m (e.what ());

        // The server threw a 416 (Requested Range Not Satisfiable). This
        // usually means our local partial file is completely mangled or we
        // actually already downloaded the whole thing and got confused trying
        // to resume. Nuke the file and retry the exact same URL from scratch.
        //
        if (m.find ("416") != string::npos && r)
        {
          error_code ec;
          fs::remove (t->request.target, ec);
          r = nullopt;

          --i; // keep the index exactly where it is for the retry.
          continue;
        }

        // We've exhausted our list of fallback mirrors. Bail out.
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
          // We have more URLs to try. Check the partial file size again in case
          // this last failed attempt managed to write some bytes before blowing
          // up.
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
