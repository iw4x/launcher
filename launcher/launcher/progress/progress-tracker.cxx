#include <launcher/progress/progress-tracker.hxx>

#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

namespace launcher
{
  namespace
  {
    inline std::uint64_t
    current_time_us () noexcept
    {
      using namespace std::chrono;
      auto now (steady_clock::now ());
      auto us (duration_cast<microseconds>(now.time_since_epoch ()));
      return static_cast<std::uint64_t>(us.count ());
    }
  }

  std::string progress_tracker::
  format_bytes (std::uint64_t n)
  {
    std::ostringstream o;

    if (n < 1024)
    {
      o << n << " B";
    }
    else if (n < 1024 * 1024)
    {
      o << std::fixed << std::setprecision (1)
        << (n / 1024.0) << " KiB";
    }
    else if (n < 1024 * 1024 * 1024)
    {
      o << std::fixed << std::setprecision (1)
        << (n / (1024.0 * 1024.0)) << " MiB";
    }
    else
    {
      o << std::fixed << std::setprecision (1)
        << (n / (1024.0 * 1024.0 * 1024.0)) << " GiB";
    }

    return o.str ();
  }

  std::string progress_tracker::
  format_speed (float bps)
  {
    std::ostringstream o;

    if (bps < 1024)
    {
      o << std::fixed << std::setprecision (0)
        << bps << " B/s";
    }
    else if (bps < 1024 * 1024)
    {
      o << std::fixed << std::setprecision (1)
        << (bps / 1024.0) << " KiB/s";
    }
    else if (bps < 1024 * 1024 * 1024)
    {
      o << std::fixed << std::setprecision (1)
        << (bps / (1024.0 * 1024.0)) << " MiB/s";
    }
    else
    {
      o << std::fixed << std::setprecision (1)
        << (bps / (1024.0 * 1024.0 * 1024.0)) << " GiB/s";
    }

    return o.str ();
  }

  std::string progress_tracker::
  format_duration (int s)
  {
    std::ostringstream o;

    int h (s / 3600);
    int m ((s % 3600) / 60);
    int sec (s % 60);

    if (h > 0)
    {
      o << h << "h" << std::setfill ('0') << std::setw (2)
        << m << "m" << std::setw (2) << sec << "s";
    }
    else
    {
      o << std::setfill ('0') << std::setw (2) << m << "m"
        << std::setw (2) << sec << "s";
    }

    return o.str ();
  }

  std::string progress_tracker::
  format_bar (float p, bool ind, int w, unsigned frame)
  {
    std::ostringstream o;
    o << "[";

    if (ind)
    {
      o << " <==> ";
      for (int i (5); i < w; ++i)
        o << " ";
    }
    else if (p == 0.0f && w > 3)
    {
      // Idle animation: bounce a <=> marker across the bar width.
      //
      int marker_len (3);  // "<=>"
      int travel (w - marker_len);
      if (travel < 1) travel = 1;

      // Ping-pong: 0..travel-1 then travel-1..0
      //
      int cycle (travel * 2);
      int pos (static_cast<int> (frame % static_cast<unsigned> (cycle)));

      if (pos >= travel)
        pos = cycle - pos;

      for (int i (0); i < w; ++i)
      {
        if (i >= pos && i < pos + marker_len)
        {
          int mi (i - pos);
          o << "<=>"[mi];
        }
        else
          o << " ";
      }
    }
    else
    {
      int filled (static_cast<int> (p * w));

      for (int i (0); i < w; ++i)
      {
        if (i < filled - 1)
          o << "=";
        else if (i == filled - 1)
          o << ">";
        else
          o << " ";
      }
    }

    o << "]";
    return o.str ();
  }

  void progress_tracker::
  update (std::uint64_t n) noexcept
  {
    std::uint64_t t (current_time_us ());
    std::uint64_t t0 (last_update_time_.load (std::memory_order_relaxed));

    std::uint64_t dt (t - t0);

    if (t0 != 0 && dt < min_update_interval_ms * 1000)
      return;

    std::uint64_t n0 (last_bytes_.load (std::memory_order_relaxed));

    float inst (0.0f);
    if (t0 != 0 && dt > 0)
    {
      std::uint64_t dn (n > n0 ? n - n0 : 0);
      inst = static_cast<float> (dn) /
             (static_cast<float> (dt) / 1000000.0f);
    }

    float s0 (speed_.load (std::memory_order_relaxed));
    float s;

    if (s0 == 0.0f)
      s = inst;
    else
      s = ewma_alpha * inst + (1.0f - ewma_alpha) * s0;

    last_bytes_.store (n, std::memory_order_relaxed);
    last_update_time_.store (t, std::memory_order_relaxed);
    speed_.store (s, std::memory_order_relaxed);

    std::size_t i (sample_index_.fetch_add (1, std::memory_order_relaxed) %
                   sample_window_size);

    samples_[i].bytes.store (n, std::memory_order_relaxed);
    samples_[i].time_us.store (t, std::memory_order_relaxed);
  }

  void progress_tracker::
  reset () noexcept
  {
    last_bytes_.store (0, std::memory_order_relaxed);
    last_update_time_.store (0, std::memory_order_relaxed);
    speed_.store (0.0f, std::memory_order_relaxed);
    sample_index_.store (0, std::memory_order_relaxed);

    for (auto& s : samples_)
    {
      s.bytes.store (0, std::memory_order_relaxed);
      s.time_us.store (0, std::memory_order_relaxed);
    }
  }

  std::string progress_tracker::
  speed_string () const
  {
    return format_speed (speed ());
  }

  std::string progress_formatter::
  format (const progress_snapshot& snapshot) const
  {
    return format (snapshot, 15);
  }

  std::string progress_formatter::
  format (const progress_snapshot& s, int w) const
  {
    std::ostringstream o;

    switch (style_)
    {
    case progress_style::simple:
      {
        int pct (static_cast<int> (s.progress_ratio () * 100));
        o << pct << "%";
        break;
      }

    case progress_style::bar:
      {
        bool i (s.total_bytes == 0);
        o << progress_tracker::format_bar (s.progress_ratio (), i, w);
        break;
      }

    case progress_style::detailed:
      {
        bool i (s.total_bytes == 0);
        int pct (static_cast<int> (s.progress_ratio () * 100));

        o << progress_tracker::format_bar (s.progress_ratio (), i, w)
          << " " << std::setw (3) << pct << "% "
          << progress_tracker::format_bytes (s.current_bytes)
          << " / "
          << progress_tracker::format_bytes (s.total_bytes)
          << " @ " << progress_tracker::format_speed (s.speed);

        int eta (s.eta_seconds ());
        if (eta > 0)
          o << " ETA: " << progress_tracker::format_duration (eta);

        break;
      }

    case progress_style::dnf:
    default:
      {
        bool i (s.total_bytes == 0);
        int pct (static_cast<int> (s.progress_ratio () * 100));

        o << progress_tracker::format_bar (s.progress_ratio (), i, w)
          << " " << std::setw (3) << pct << "% "
          << std::setw (10) << std::left
          << progress_tracker::format_speed (s.speed);

        int eta (s.eta_seconds ());
        if (eta > 0)
          o << " " << std::setw (8)
            << progress_tracker::format_duration (eta);

        break;
      }
    }

    return o.str ();
  }
}
