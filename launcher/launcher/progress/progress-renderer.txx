#include <sstream>
#include <iomanip>

namespace launcher
{
  using namespace ftxui;

  template <typename S>
  ftxui::Element progress_renderer_traits<S>::
  render_item (const string_type& label,
               const progress_snapshot& s,
               int w)
  {
    using traits = progress_tracker_traits<S>;

    // Calculate the progress state.
    //
    // Note that if we have 0 total bytes, we treat it as indeterminate
    // (bouncing bar) unless the task is strictly inactive.
    //
    bool i (s.total_bytes == 0 ||
            (s.state == progress_state::active && s.total_bytes == 0));

    float p (s.progress_ratio ());
    int pct (static_cast<int> (p * 100));

    // Format the label.
    //
    std::ostringstream l;
    l << label;

    // Format the stats (right side).
    //
    // We use fixed widths to prevent the layout from jittering as numbers
    // change.
    //
    std::ostringstream r;
    r << std::right << std::setw (4) << pct << "%"
      << " " << traits::format_bar (p, i, w)
      << " | " << std::setw (12) << traits::format_speed (s.speed)
      << " | " << std::setw (10) << traits::format_bytes (s.current_bytes)
      << " | " << std::setw (7);

    // Append ETA.
    //
    int eta (s.eta_seconds ());

    if (eta > 0)
      r << traits::format_duration (eta);
    else
      r << "0m00s";

    return hbox ({
      text (l.str ()),
      filler (),
      text (r.str ())
    });
  }

  template <typename S>
  ftxui::Element progress_renderer_traits<S>::
  render_summary (std::size_t done,
                  std::size_t total,
                  const progress_snapshot& s)
  {
    using traits = progress_tracker_traits<S>;

    // Calculate overall progress.
    //
    // If the total byte count is zero (e.g., we are just counting files or
    // haven't started the transfer yet), we render an indeterminate bar to
    // show activity without implying a specific percentage.
    //
    bool i (s.total_bytes == 0);
    float p (s.progress_ratio ());
    int pct (static_cast<int> (p * 100));

    // Format the summary label.
    //
    // We display the [N/M] item count on the left. We pad the right side
    // of this string to ensure the "Total" label stands apart from the
    // bar.
    //
    std::ostringstream l;
    l << "[" << std::setw (2) << done << "/"
      << std::setw (2) << total << "] Total"
      << std::left << std::setw (40) << "";

    // Format the stats.
    //
    std::ostringstream r;
    r << std::right << std::setw (4) << pct << "%"
      << " " << traits::format_bar (p, i, default_bar_width)
      << " | " << std::setw (12) << traits::format_speed (s.speed)
      << " | " << std::setw (10) << traits::format_bytes (s.current_bytes)
      << " | " << std::setw (7);

    // Append ETA.
    //
    int eta (s.eta_seconds ());

    if (eta > 0)
      r << traits::format_duration (eta);
    else
      r << "0m00s";

    return hbox ({
      text (l.str ()),
      filler (),
      text (r.str ())
    });
  }

  template <typename S>
  ftxui::Element progress_renderer_traits<S>::
  render_logs (const std::vector<string_type>& msgs)
  {
    // Short-circuit if empty.
    //
    // FTXUI's vbox doesn't like empty lists (or rather, it's just waste),
    // so we return a simple empty text node.
    //
    if (msgs.empty ())
      return text ("");

    Elements es;
    es.reserve (msgs.size ());

    // Convert strings to UI elements.
    //
    // We preserve the order (oldest to newest) as they appear in the buffer.
    //
    for (const auto& m: msgs)
      es.push_back (text (m));

    return vbox (std::move (es));
  }

  template <typename S>
  ftxui::Element progress_renderer_traits<S>::
  render_status (const string_type& s)
  {
    if (s.empty ())
      return text ("");

    // Style the status.
    //
    // We use bold green text to make the current high-level operation (e.g.,
    // "Verifying...", "Cleaning up") stand out visually against the scrolling
    // logs and the progress bars.
    //
    return text (s) | bold | color (Color::Green);
  }

  // basic_progress_renderer implementation.
  //
  template <typename T>
  basic_progress_renderer<T>::
  basic_progress_renderer ()
      : screen_ (ftxui::ScreenInteractive::Fullscreen ())
  {
  }

  template <typename T>
  basic_progress_renderer<T>::
  ~basic_progress_renderer ()
  {
    if (running ())
      stop ();
  }

  template <typename T>
  void basic_progress_renderer<T>::
  start ()
  {
    if (running_.exchange (true, std::memory_order_relaxed))
      return;

    component_ = create_component ();

    // Run the UI loop in a background thread.
    //
    // Note that ScreenInteractive::Loop() is blocking, so we cannot run it
    // on the main ASIO thread.
    //
    ui_thread_ = std::jthread ([this]
    {
      screen_.Loop (component_);
      running_.store (false, std::memory_order_relaxed);
    });
  }

  template <typename T>
  void basic_progress_renderer<T>::
  stop ()
  {
    if (!running_.exchange (false, std::memory_order_relaxed))
      return;

    screen_.Exit ();
  }

  template <typename T>
  void basic_progress_renderer<T>::
  update (context_type&& ctx) noexcept
  {
    // Write to the back buffer.
    //
    int r (render_buffer_.load (std::memory_order_relaxed));
    int w ((r + 1) % 2);

    contexts_[w] = std::move (ctx);

    render_buffer_.store (w, std::memory_order_release);

    refresh ();
  }

  template <typename T>
  void basic_progress_renderer<T>::
  refresh () noexcept
  {
    if (component_ && running ())
      screen_.Post (ftxui::Event::Custom);
  }

  template <typename T>
  ftxui::Component basic_progress_renderer<T>::
  create_component ()
  {
    return ftxui::Renderer ([this]
    {
      // Acquire the context.
      //
      int r (render_buffer_.load (std::memory_order_acquire));
      const context_type& c (contexts_[r]);

      // Calculate the vertical space available for the item list.
      //
      // We reserve lines for the summary (1), summary separator (1),
      // and truncate message (2, if present).
      //
      int h (screen_.dimy ());
      int rh (3);

      int ah (std::max (0, h - rh));

      Elements es;

      // Render the items or logs.
      //
      // When no items are present yet, we show the log messages in the main
      // area to provide feedback during initialization. Once items appear,
      // we hide the logs and show the progress list instead.
      //
      if (c.items.empty () && !c.log_messages.empty ())
      {
        // Show logs in the main area.
        //
        for (const auto& m : c.log_messages)
          es.push_back (text (m));
      }
      else
      {
        // Render the items.
        //
        // If the list is too long for the terminal, we truncate it and display
        // a count of hidden items.
        //
        std::size_t n (0);
        std::size_t max (std::min (c.items.size (),
                                   static_cast<std::size_t> (ah)));

        for (std::size_t i (0); i < max; ++i)
        {
          const auto& item (c.items[i]);
          ++n;

          std::ostringstream l;
          l << "[" << std::setw (2) << n << "/"
            << std::setw (2) << c.total_count << "] "
            << std::left << std::setw (45) << item.label;

          es.push_back (traits_type::render_item (
            l.str (),
            item.snapshot,
            traits_type::default_bar_width));
        }

        if (max < c.items.size ())
        {
          std::size_t k (c.items.size () - max);
          std::ostringstream s;
          s << "... (" << k << " more files not shown)";
          es.push_back (text (s.str ()) | dim);
        }
      }

      // Render the summary.
      //
      Elements bottom_es;
      bottom_es.push_back (separator ());
      bottom_es.push_back (traits_type::render_summary (
        c.completed_count,
        c.total_count,
        c.overall));

      return vbox ({
        vbox (std::move (es)) | flex | yframe,
        vbox (std::move (bottom_es))
      });
    });
  }
}
