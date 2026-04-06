#include <launcher/progress/progress-renderer.hxx>
#include <launcher/progress/progress-tracker.hxx>

#include <sstream>
#include <iomanip>
#include <algorithm>

namespace launcher
{
  using namespace ftxui;

  ftxui::Element progress_renderer::
  render_item (const std::string& label,
               const progress_snapshot& s,
               int w,
               unsigned frame)
  {
    bool i (s.total_bytes == 0 ||
            (s.state == progress_state::active && s.total_bytes == 0));

    float p (s.progress_ratio ());
    int pct (static_cast<int> (p * 100));

    std::ostringstream l;
    l << label;

    std::ostringstream r;
    r << std::right << std::setw (4) << pct << "%"
      << " " << progress_tracker::format_bar (p, i, w, frame)
      << " | " << std::setw (12) << progress_tracker::format_speed (s.speed)
      << " | " << std::setw (10) << progress_tracker::format_bytes (s.current_bytes)
      << " | " << std::setw (7);

    int eta (s.eta_seconds ());

    if (eta > 0)
      r << progress_tracker::format_duration (eta);
    else
      r << "0m00s";

    return hbox ({
      text (l.str ()),
      filler (),
      text (r.str ())
    });
  }

  ftxui::Element progress_renderer::
  render_summary (std::size_t done,
                  std::size_t total,
                  const progress_snapshot& s,
                  unsigned frame)
  {
    bool i (s.total_bytes == 0);
    float p (s.progress_ratio ());
    int pct (static_cast<int> (p * 100));

    std::ostringstream l;
    l << "[" << std::setw (2) << done << "/"
      << std::setw (2) << total << "] Total"
      << std::left << std::setw (40) << "";

    std::ostringstream r;
    r << std::right << std::setw (4) << pct << "%"
      << " " << progress_tracker::format_bar (p, i, default_bar_width, frame)
      << " | " << std::setw (12) << progress_tracker::format_speed (s.speed)
      << " | " << std::setw (10) << progress_tracker::format_bytes (s.current_bytes)
      << " | " << std::setw (7);

    int eta (s.eta_seconds ());

    if (eta > 0)
      r << progress_tracker::format_duration (eta);
    else
      r << "0m00s";

    return hbox ({
      text (l.str ()),
      filler (),
      text (r.str ())
    });
  }

  ftxui::Element progress_renderer::
  render_logs (const std::vector<std::string>& msgs)
  {
    if (msgs.empty ())
      return text ("");

    Elements es;
    es.reserve (msgs.size ());

    for (const auto& m: msgs)
      es.push_back (text (m));

    return vbox (std::move (es));
  }

  ftxui::Element progress_renderer::
  render_status (const std::string& s)
  {
    if (s.empty ())
      return text ("");

    return text (s) | bold | color (Color::Green);
  }

  progress_renderer::
  progress_renderer ()
      : screen_ (ftxui::ScreenInteractive::Fullscreen ())
  {
  }

  progress_renderer::
  ~progress_renderer ()
  {
    if (running ())
      stop ();
  }

  void progress_renderer::
  start ()
  {
    if (running_.exchange (true, std::memory_order_relaxed))
      return;

    component_ = create_component ();

    ui_thread_ = std::jthread ([this]
    {
      screen_.Loop (component_);
      running_.store (false, std::memory_order_relaxed);
    });
  }

  void progress_renderer::
  stop ()
  {
    if (!running_.exchange (false, std::memory_order_relaxed))
      return;

    screen_.Exit ();
  }

  void progress_renderer::
  update (progress_render_context&& ctx) noexcept
  {
    int r (render_buffer_.load (std::memory_order_relaxed));
    int w ((r + 1) % 2);

    contexts_[w] = std::move (ctx);

    render_buffer_.store (w, std::memory_order_release);

    refresh ();
  }

  void progress_renderer::
  refresh () noexcept
  {
    if (component_ && running ())
      screen_.Post (ftxui::Event::Custom);
  }

  ftxui::Component progress_renderer::
  create_component ()
  {
    return ftxui::Renderer ([this]
    {
      int r (render_buffer_.load (std::memory_order_acquire));
      const progress_render_context& c (contexts_[r]);

      int h (screen_.dimy ());
      int rh (3);

      int ah (std::max (0, h - rh));

      Elements es;

      if (c.items.empty () && !c.log_messages.empty ())
      {
        for (const auto& m : c.log_messages)
          es.push_back (text (m));
      }
      else
      {
        std::size_t n (c.completed_count);
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

          es.push_back (render_item (
            l.str (),
            item.snapshot,
            default_bar_width,
            c.frame));
        }

        if (max < c.items.size ())
        {
          std::size_t k (c.items.size () - max);
          std::ostringstream s;
          s << "... (" << k << " more files not shown)";
          es.push_back (text (s.str ()) | dim);
        }
      }

      Elements bottom_es;
      bottom_es.push_back (separator ());
      bottom_es.push_back (render_summary (
        c.completed_count,
        c.total_count,
        c.overall,
        c.frame));

      Element main_view = vbox ({
        vbox (std::move (es)) | flex | yframe,
        vbox (std::move (bottom_es))
      });

      if (c.dialog_visible)
      {
        Element content (
          vbox ({
            text (c.dialog_title) | bold | center,
            separator (),
            text (c.dialog_message) | center
          }) | border | size (WIDTH, GREATER_THAN, 50) | size (HEIGHT, GREATER_THAN, 7));

        return dbox ({
          main_view | dim,
          content | center
        });
      }

      return main_view;
    });
  }
}
