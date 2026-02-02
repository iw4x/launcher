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
  progress_coordinator (asio::io_context& c)
    : ioc_ (c),
      manager_ (make_unique<manager_type> (c))
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
  add_entry (string l)
  {
    return manager_->add_entry (move (l));
  }

  void progress_coordinator::
  remove_entry (shared_ptr<entry_type> e)
  {
    manager_->remove_entry (move (e));
  }

  void progress_coordinator::
  update_progress (shared_ptr<entry_type> e, uint64_t c, uint64_t t)
  {
    // We use relaxed memory order for metric updates. Since this data is
    // purely for UI feedback, observing a value that is a few CPU cycles
    // stale is an acceptable trade-off for minimizing synchronization
    // overhead.
    //
    auto& m (e->metrics ());
    m.current_bytes.store (c, memory_order_relaxed);
    m.total_bytes.store (t, memory_order_relaxed);

    // Determine state.
    //
    // Note that we assume completion if we've hit the total bytes, provided
    // we actually expect some data (t > 0).
    //
    progress_state s ((t > 0 && c >= t)
                      ? progress_state::completed
                      : progress_state::active);

    m.state.store (s, memory_order_relaxed);

    // Update tracker speed.
    //
    auto& tr (e->tracker ());
    tr.update (c);

    m.speed.store (tr.speed (), memory_order_relaxed);
  }

  void progress_coordinator::
  add_log (string m)
  {
    manager_->add_log (move (m));
  }

  void progress_coordinator::
  show_dialog (string t, string m)
  {
    manager_->show_dialog (move (t), move (m));
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
    // Snapshot the atomics. While relaxed ordering doesn't guarantee a
    // strictly consistent view across all variables, loading them upfront
    // gives us a "good enough" point-in-time for the string generation.
    //
    uint64_t c (m.current_bytes.load (memory_order_relaxed));
    uint64_t t (m.total_bytes.load (memory_order_relaxed));
    float s (m.speed.load (memory_order_relaxed));

    ostringstream o;
    o << format_bytes (c) << " / " << format_bytes (t);

    // Calculate percentage if we have a target.
    //
    if (t > 0)
    {
      float r (static_cast<float> (c) / static_cast<float> (t));
      o << " (" << fixed << setprecision (1) << (r * 100.0f) << "%)";
    }

    // Append speed and ETA.
    //
    if (s > 0.0f)
    {
      o << " @ " << format_speed (s);

      int eta (m.eta_seconds ());
      if (eta > 0)
        o << ", ETA " << format_duration (eta);
    }

    return o.str ();
  }

  string
  format_bytes (uint64_t b)
  {
    // Stick to standard IEC binary prefixes (KiB, MiB, etc.) rather than
    // SI decimal ones since we are dealing with file sizes and buffers.
    //
    static const char* u[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    static const size_t n (sizeof (u) / sizeof (*u));

    double v (static_cast<double> (b));
    size_t i (0);

    while (v >= 1024.0 && i < n - 1)
    {
      v /= 1024.0;
      ++i;
    }

    ostringstream o;
    o << fixed << setprecision (1) << v << ' ' << u[i];
    return o.str ();
  }

  string
  format_speed (float bps)
  {
    return format_bytes (static_cast<uint64_t> (bps)) + "/s";
  }

  string
  format_duration (int s)
  {
    ostringstream o;

    if (s < 60)
    {
      o << s << "s";
    }
    else if (s < 3600)
    {
      // Minutes and remainder seconds.
      //
      int m (s / 60);
      int r (s % 60);

      o << m << "m";
      if (r > 0) o << ' ' << r << "s";
    }
    else
    {
      // Once we hit hours, second-level precision is mostly noise, so we
      // drop it.
      //
      int h (s / 3600);
      int m ((s % 3600) / 60);

      o << h << "h";
      if (m > 0) o << ' ' << m << "m";
    }

    return o.str ();
  }

  string
  format_progress_bar (float r, int w, bool ind)
  {
    ostringstream o;
    o << '[';

    if (ind)
    {
      // For indeterminate states, we just throb in the center to show
      // liveness.
      //
      for (int i (0); i < w; ++i)
        o << (i == w / 2 ? '>' : ' ');
    }
    else
    {
      int f (static_cast<int> (round (r * w)));
      f = min (f, w);

      // Render the standard arrow: '===>   '
      //
      for (int i (0); i < w; ++i)
      {
        if      (i <  f - 1) o << '=';
        else if (i == f - 1) o << '>';
        else                 o << ' ';
      }
    }

    o << ']';
    return o.str ();
  }
}
