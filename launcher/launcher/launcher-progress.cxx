#include <launcher/launcher-progress.hxx>

#include <cmath>
#include <iomanip>
#include <sstream>

#include <launcher/progress/progress-manager.hxx>
#include <launcher/progress/progress-tracker.hxx>

using namespace std;

namespace launcher
{
  progress_coordinator::
  progress_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      manager_ (make_unique<manager_type> (ioc))
  {
  }

  void progress_coordinator::
  start ()
  {
    manager_->start ();
  }

  asio::awaitable<void> progress_coordinator::
  stop ()
  {
    co_await manager_->stop ();
  }

  bool progress_coordinator::
  running () const noexcept
  {
    return manager_->running ();
  }

  shared_ptr<progress_coordinator::entry_type> progress_coordinator::
  add_entry (string label)
  {
    return manager_->add_entry (move (label));
  }

  void progress_coordinator::
  remove_entry (shared_ptr<entry_type> e)
  {
    manager_->remove_entry (move (e));
  }

  void progress_coordinator::
  update_progress (shared_ptr<entry_type> e,
                   uint64_t current,
                   uint64_t total)
  {
    // Note that we're using relaxed memory order because these values are only
    // used for feedback. If the user sees a value that is a few CPU cycles
    // stale, it's not the end of the world. We prefer performance here.
    //
    auto& m (e->metrics ());
    m.current_bytes.store (current, memory_order_relaxed);
    m.total_bytes.store (total, memory_order_relaxed);

    // Determine the state.
    //
    progress_state s ((total > 0 && current >= total)
                        ? progress_state::completed
                        : progress_state::active);

    m.state.store (s, memory_order_relaxed);

    // Update the tracker speed calculation.
    //
    auto& t (e->tracker ());
    t.update (current);

    m.speed.store (t.speed (), memory_order_relaxed);
  }

  void progress_coordinator::
  add_log (string m)
  {
    manager_->add_log (move (m));
  }

  void progress_coordinator::
  show_dialog (string title, string message)
  {
    manager_->show_dialog (move (title), move (message));
  }

  void progress_coordinator::
  hide_dialog ()
  {
    manager_->hide_dialog ();
  }

  progress_coordinator::manager_type& progress_coordinator::
  manager () noexcept
  {
    return *manager_;
  }

  const progress_coordinator::manager_type& progress_coordinator::
  manager () const noexcept
  {
    return *manager_;
  }

  // Formatting
  //

  string
  format_progress (const progress_metrics& m)
  {
    // Load everything upfront so that we have a consistent snapshot (or as
    // close as we can get with relaxed ordering).
    //
    uint64_t cur (m.current_bytes.load (memory_order_relaxed));
    uint64_t tot (m.total_bytes.load (memory_order_relaxed));
    float spd (m.speed.load (memory_order_relaxed));

    ostringstream oss;
    oss << format_bytes (cur) << " / " << format_bytes (tot);

    // If we have a total, we can calculate the percentage.
    //
    if (tot > 0)
    {
      float r (static_cast<float> (cur) / static_cast<float> (tot));
      oss << " (" << fixed << setprecision (1) << (r * 100.0f) << "%)";
    }

    // Append speed and ETA if available.
    //
    if (spd > 0.0f)
    {
      oss << " @ " << format_speed (spd);

      int eta (m.eta_seconds ());
      if (eta > 0)
        oss << ", ETA " << format_duration (eta);
    }

    return oss.str ();
  }

  string
  format_bytes (uint64_t b)
  {
    // Keep it simple: standard SI prefixes.
    //
    static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    static const size_t n_units (sizeof (units) / sizeof (*units));

    double v (static_cast<double> (b));
    size_t i (0);

    while (v >= 1024.0 && i < n_units - 1)
    {
      v /= 1024.0;
      ++i;
    }

    ostringstream oss;
    oss << fixed << setprecision (1) << v << ' ' << units[i];
    return oss.str ();
  }

  string
  format_speed (float bps)
  {
    return format_bytes (static_cast<uint64_t> (bps)) + "/s";
  }

  string
  format_duration (int s)
  {
    ostringstream oss;

    if (s < 60)
    {
      oss << s << "s";
    }
    else if (s < 3600)
    {
      int m (s / 60);
      int r (s % 60);

      oss << m << "m";
      if (r > 0) oss << ' ' << r << "s";
    }
    else
    {
      // Good enough for long durations: hours and minutes.
      //
      int h (s / 3600);
      int m ((s % 3600) / 60);

      oss << h << "h";
      if (m > 0) oss << ' ' << m << "m";
    }

    return oss.str ();
  }

  string
  format_progress_bar (float ratio, int width, bool indeterminate)
  {
    ostringstream oss;
    oss << '[';

    if (indeterminate)
    {
      // If we don't know the progress, just throb in the middle.
      //
      for (int i (0); i < width; ++i)
        oss << (i == width / 2 ? '>' : ' ');
    }
    else
    {
      int filled (static_cast<int> (round (ratio * width)));
      filled = min (filled, width);

      // Draw the bar: '===>   '
      //
      for (int i (0); i < width; ++i)
      {
        if      (i <  filled - 1) oss << '=';
        else if (i == filled - 1) oss << '>';
        else                      oss << ' ';
      }
    }

    oss << ']';
    return oss.str ();
  }
}
