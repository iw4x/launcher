#include <algorithm>

namespace hello
{
  // basic_progress_manager implementation.
  //
  template <typename T>
  basic_progress_manager<T>::
  basic_progress_manager (asio::io_context& ioc)
      : ioc_ (ioc),
        update_timer_ (ioc),
        render_timer_ (ioc),
        strand_ (asio::make_strand (ioc))
  {
  }

  template <typename T>
  basic_progress_manager<T>::
  ~basic_progress_manager ()
  {
    if (running ())
    {
      // Synchronous stop for destructor.
      //
      running_.store (false, std::memory_order_relaxed);
      update_timer_.cancel ();
      render_timer_.cancel ();
      renderer_.stop ();
    }
  }

  template <typename T>
  void basic_progress_manager<T>::
  start ()
  {
    // Try to transition from stopped to running. If we are already running,
    // just bail out.
    //
    if (running_.exchange (true, std::memory_order_relaxed))
      return;

    renderer_.start ();

    // Launch the background loops.
    //
    // We spawn them on the strand so that their internal handlers (like timer
    // waits) are serialized with other manager operations if necessary, though
    // the loops themselves are detached.
    //
    asio::co_spawn (strand_, update_loop (), asio::detached);
    asio::co_spawn (strand_, render_loop (), asio::detached);
  }

  template <typename T>
  asio::awaitable<void> basic_progress_manager<T>::
  stop ()
  {
    if (!running_.exchange (false, std::memory_order_relaxed))
      co_return;

    // Cancel the timers to force the loops to wake up and exit immediately.
    //
    update_timer_.cancel ();
    render_timer_.cancel ();

    renderer_.stop ();

    co_return;
  }

  template <typename T>
  std::shared_ptr<typename basic_progress_manager<T>::entry_type>
  basic_progress_manager<T>::
  add_entry (string_type l)
  {
    auto e (std::make_shared<entry_type> (std::move (l)));

    // Schedule the addition on the strand.
    //
    // Since we are modifying the vector structure itself (not just the atomic
    // metrics inside an entry), we need single-writer semantics.
    //
    asio::post (strand_,
                [this, e]
    {
      // Double-buffering logic.
      //
      // We maintain two buffers: one that is currently being read by the
      // update/render loops (r) and one that is idle (w). We write to the
      // idle one and then swap.
      //
      int r (entries_buffer_.load (std::memory_order_relaxed));
      int w ((r + 1) % 2);

      // Copy the current state to the back buffer and append the new entry.
      //
      entries_buffers_ [w] = entries_buffers_ [r];
      entries_buffers_ [w].push_back (e);

      // Publish the new state.
      //
      // Note that we use release memory order so that the write to the vector
      // is visible to any thread that loads the index with acquire.
      //
      entries_buffer_.store (w, std::memory_order_release);

      overall_metrics_.total_items.fetch_add (1, std::memory_order_relaxed);
    });

    return e;
  }

  template <typename T>
  void basic_progress_manager<T>::
  remove_entry (std::shared_ptr<entry_type> e)
  {
    asio::post (strand_,
                [this, e]
    {
      int r (entries_buffer_.load (std::memory_order_relaxed));
      int w ((r + 1) % 2);

      // Copy-on-write: replicate the current state to the back buffer.
      //
      entries_buffers_ [w] = entries_buffers_ [r];

      auto i (std::find (entries_buffers_ [w].begin (),
                         entries_buffers_ [w].end (),
                         e));

      if (i != entries_buffers_ [w].end ())
      {
        entries_buffers_ [w].erase (i);

        entries_buffer_.store (w, std::memory_order_release);

        // If the item wasn't actually completed when we removed it (e.g.,
        // cancelled or removed early), we still count it as "processed" in
        // the overall stats to avoid the totals looking weird.
        //
        if (e->metrics ().state.load (std::memory_order_relaxed) !=
            progress_state::completed)
        {
          overall_metrics_.completed_items.fetch_add (
            1,
            std::memory_order_relaxed);
        }
      }
    });
  }

  // @@: Previously used for debugging and currently retained for reference.
  //
  template <typename T>
  void basic_progress_manager<T>::
  add_log (string_type m)
  {
    // Note: We assume this is called from a context that doesn't race with
    // other log writers (e.g., the strand), or that the caller accepts the
    // risk.
    //
    int r (log_buffer_.load (std::memory_order_relaxed));
    int w ((r + 1) % 2);

    log_buffers_ [w].push_back (std::move (m));

    // Cap the log history to prevent unbounded growth.
    //
    if (log_buffers_ [w].size () >
        progress_renderer_traits<string_type>::max_log_messages)
      log_buffers_ [w].erase (log_buffers_ [w].begin ());

    log_buffer_.store (w, std::memory_order_release);
  }

  template <typename T>
  void basic_progress_manager<T>::
  set_status (string_type s)
  {
    int r (status_buffer_.load (std::memory_order_relaxed));
    int w ((r + 1) % 2);

    status_buffers_ [w] = std::move (s);

    status_buffer_.store (w, std::memory_order_release);
  }

  template <typename T>
  asio::awaitable<void> basic_progress_manager<T>::
  update_loop ()
  {
    while (running_.load (std::memory_order_relaxed))
    {
      // Acquire the current read buffer index.
      //
      // We don't lock here. If `add_entry` happens while we are iterating,
      // we just process the "old" list one last time. We'll pick up the new
      // entry on the next tick.
      //
      int r (entries_buffer_.load (std::memory_order_acquire));
      const auto& entries (entries_buffers_[r]);

      std::uint64_t total (0);
      std::uint64_t current (0);
      std::uint64_t completed (0);
      float speed_sum (0.0f);

      // Iterate over the snapshot.
      //
      // Note that while the list of entries (`entries`) is a snapshot, the
      // metrics *inside* each entry are atomic. This means we are reading
      // the live byte counts and states, even if the list structure itself
      // is slightly stale.
      //
      for (const auto& e : entries)
      {
        auto& m (e->metrics ());
        auto& t (e->tracker ());

        std::uint64_t c (m.current_bytes.load (std::memory_order_relaxed));
        std::uint64_t to (m.total_bytes.load (std::memory_order_relaxed));

        // Update the sliding window tracker for this entry.
        //
        t.update (c);

        float s (t.speed ());
        m.speed.store (s, std::memory_order_relaxed);

        total += to;
        current += c;
        speed_sum += s;

        if (m.state.load (std::memory_order_relaxed) ==
            progress_state::completed)
          ++completed;
      }

      // Update the overall metrics.
      //
      // These are atomic stores, so the render loop (which might be running
      // on another thread) will see consistent values.
      //
      auto& om (overall_metrics_);
      om.total_bytes.store (total, std::memory_order_relaxed);
      om.current_bytes.store (current, std::memory_order_relaxed);
      om.speed.store (speed_sum, std::memory_order_relaxed);
      om.completed_items.store (completed, std::memory_order_relaxed);

      // Sleep until the next tick.
      //
      update_timer_.expires_after (
        std::chrono::milliseconds (traits_type::update_interval_ms));

      try
      {
        co_await update_timer_.async_wait (asio::use_awaitable);
      }
      catch (const boost::system::system_error& e)
      {
        if (e.code () == asio::error::operation_aborted)
          break; // manager stopped

        throw;
      }
    }

    co_return;
  }

  template <typename T>
  asio::awaitable<void> basic_progress_manager<T>::
  render_loop ()
  {
    while (running_.load (std::memory_order_relaxed))
    {
      // Collect the full context.
      //
      // This involves atomic loads and vector copies, but it gives the
      // renderer a stable, immutable snapshot to work with.
      //
      context_type ctx (collect_context ());

      renderer_.update (std::move (ctx));

      render_timer_.expires_after (
        std::chrono::milliseconds (traits_type::render_interval_ms));

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

  template <typename T>
  typename basic_progress_manager<T>::context_type
  basic_progress_manager<T>::
  collect_context ()
  {
    context_type ctx;

    // Read from the active buffer.
    //
    // Note that we use acquire semantics here so that if the producer thread
    // has just swapped the buffers (released), we see the fully constructed
    // vector contents.
    //
    int r (entries_buffer_.load (std::memory_order_acquire));
    const auto& entries (entries_buffers_[r]);

    // Snapshot individual entries.
    //
    for (const auto& e : entries)
    {
      basic_progress_item<string_type> item (e->label (), e->snapshot ());
      ctx.items.push_back (std::move (item));
    }

    // Snapshot atomics directly.
    //
    ctx.overall = progress_snapshot (overall_metrics_);
    ctx.completed_count = overall_metrics_.completed_items.load (
      std::memory_order_relaxed);
    ctx.total_count = overall_metrics_.total_items.load (
      std::memory_order_relaxed);

    // Logs have their own independent buffer swap cycle.
    //
    int lr (log_buffer_.load (std::memory_order_acquire));
    ctx.log_messages = log_buffers_[lr];

    // Status: Read from the active buffer.
    //
    int sr (status_buffer_.load (std::memory_order_acquire));
    ctx.status_message = status_buffers_[sr];

    return ctx;
  }
}
