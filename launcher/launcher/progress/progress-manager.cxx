#include <launcher/progress/progress-manager.hxx>
#include <algorithm>

namespace launcher
{
  progress_manager::
  progress_manager (asio::io_context& ioc)
      : ioc_ (ioc),
        update_timer_ (ioc),
        render_timer_ (ioc),
        strand_ (asio::make_strand (ioc)),
        dialog_strand_ (asio::make_strand (ioc))
  {
  }

  progress_manager::
  ~progress_manager ()
  {
    if (running ())
    {
      running_.store (false, std::memory_order_relaxed);
      update_timer_.cancel ();
      render_timer_.cancel ();
      renderer_.stop ();
    }
  }

  void progress_manager::
  start ()
  {
    if (running_.exchange (true, std::memory_order_relaxed))
      return;

    renderer_.start ();

    asio::co_spawn (strand_, update_loop (), asio::detached);
    asio::co_spawn (strand_, render_loop (), asio::detached);
  }

  asio::awaitable<void> progress_manager::
  stop ()
  {
    if (!running_.exchange (false, std::memory_order_relaxed))
      co_return;

    update_timer_.cancel ();
    render_timer_.cancel ();

    renderer_.stop ();

    co_return;
  }

  std::shared_ptr<progress_entry> progress_manager::
  add_entry (std::string l)
  {
    auto e (std::make_shared<progress_entry> (std::move (l)));

    asio::post (strand_,
                [this, e]
    {
      int r (entries_buffer_.load (std::memory_order_relaxed));
      int w ((r + 1) % 2);

      entries_buffers_ [w] = entries_buffers_ [r];
      entries_buffers_ [w].push_back (e);

      entries_buffer_.store (w, std::memory_order_release);

      overall_metrics_.total_items.fetch_add (1, std::memory_order_relaxed);
    });

    return e;
  }

  void progress_manager::
  remove_entry (std::shared_ptr<progress_entry> e)
  {
    asio::post (strand_,
                [this, e]
    {
      int r (entries_buffer_.load (std::memory_order_relaxed));
      int w ((r + 1) % 2);

      entries_buffers_ [w] = entries_buffers_ [r];

      auto i (std::find (entries_buffers_ [w].begin (),
                         entries_buffers_ [w].end (),
                         e));

      if (i != entries_buffers_ [w].end ())
      {
        entries_buffers_ [w].erase (i);

        entries_buffer_.store (w, std::memory_order_release);

        if (e->metrics ().state.load (std::memory_order_relaxed) ==
            progress_state::completed)
        {
          overall_metrics_.completed_items.fetch_add (
            1,
            std::memory_order_relaxed);

          std::uint64_t item_total_bytes (
            e->metrics ().total_bytes.load (std::memory_order_relaxed));

          cumulative_completed_bytes_.fetch_add (
            item_total_bytes,
            std::memory_order_relaxed);
        }
      }
    });
  }

  void progress_manager::
  add_log (std::string m)
  {
    int r (log_buffer_.load (std::memory_order_relaxed));
    int w ((r + 1) % 2);

    log_buffers_ [w].push_back (std::move (m));

    if (log_buffers_ [w].size () > progress_renderer::max_log_messages)
      log_buffers_ [w].erase (log_buffers_ [w].begin ());

    log_buffer_.store (w, std::memory_order_release);
  }

  void progress_manager::
  show_dialog (std::string title, std::string message)
  {
    asio::post (dialog_strand_,
                [this, t = std::move (title), m = std::move (message)] ()
    {
      dialog_title_ = std::move (t);
      dialog_message_ = std::move (m);
      dialog_visible_.store (true, std::memory_order_release);
    });
  }

  void progress_manager::
  hide_dialog ()
  {
    dialog_visible_.store (false, std::memory_order_release);
  }

  asio::awaitable<void> progress_manager::
  update_loop ()
  {
    while (running_.load (std::memory_order_relaxed))
    {
      int r (entries_buffer_.load (std::memory_order_acquire));
      const auto& entries (entries_buffers_[r]);

      std::uint64_t total (0);
      std::uint64_t current (0);
      float speed_sum (0.0f);

      for (const auto& e : entries)
      {
        auto& m (e->metrics ());
        auto& t (e->tracker ());

        std::uint64_t c (m.current_bytes.load (std::memory_order_relaxed));
        std::uint64_t to (m.total_bytes.load (std::memory_order_relaxed));

        t.update (c);

        float s (t.speed ());
        m.speed.store (s, std::memory_order_relaxed);

        total += to;
        current += c;
        speed_sum += s;
      }

      auto& om (overall_metrics_);

      std::uint64_t max_total (
        cumulative_total_bytes_.load (std::memory_order_relaxed));

      std::uint64_t removed (
        cumulative_completed_bytes_.load (std::memory_order_relaxed));

      std::uint64_t observed (total + removed);

      while (observed > max_total)
      {
        if (cumulative_total_bytes_.compare_exchange_weak (
              max_total,
              observed,
              std::memory_order_relaxed))
        {
          max_total = observed;
          break;
        }

        observed = total + removed;
      }

      std::uint64_t abs_current (removed + current);

      om.total_bytes.store (max_total, std::memory_order_relaxed);
      om.current_bytes.store (abs_current, std::memory_order_relaxed);

      overall_tracker_.update (abs_current);
      om.speed.store (overall_tracker_.speed (), std::memory_order_relaxed);

      update_timer_.expires_after (
        std::chrono::milliseconds (update_interval_ms));

      try
      {
        co_await update_timer_.async_wait (asio::use_awaitable);
      }
      catch (const boost::system::system_error& e)
      {
        if (e.code () == asio::error::operation_aborted)
          break;

        throw;
      }
    }

    co_return;
  }

  asio::awaitable<void> progress_manager::
  render_loop ()
  {
    while (running_.load (std::memory_order_relaxed))
    {
      progress_render_context ctx (collect_context ());

      renderer_.update (std::move (ctx));

      render_timer_.expires_after (
        std::chrono::milliseconds (render_interval_ms));

      try
      {
        co_await render_timer_.async_wait (asio::use_awaitable);
      }
      catch (const boost::system::system_error& e)
      {
        if (e.code () == asio::error::operation_aborted)
          break;

        throw;
      }
    }

    co_return;
  }

  progress_render_context progress_manager::
  collect_context ()
  {
    progress_render_context ctx;

    int r (entries_buffer_.load (std::memory_order_acquire));
    const auto& entries (entries_buffers_[r]);

    for (const auto& e : entries)
    {
      progress_item item (e->label (), e->snapshot ());
      ctx.items.push_back (std::move (item));
    }

    ctx.overall = progress_snapshot (overall_metrics_);
    ctx.completed_count = overall_metrics_.completed_items.load (
      std::memory_order_relaxed);
    ctx.total_count = overall_metrics_.total_items.load (
      std::memory_order_relaxed);

    int lr (log_buffer_.load (std::memory_order_acquire));
    ctx.log_messages = log_buffers_[lr];

    ctx.dialog_visible = dialog_visible_.load (std::memory_order_acquire);
    if (ctx.dialog_visible)
      ctx.dialog_title = dialog_title_, ctx.dialog_message = dialog_message_;

    return ctx;
  }
}
