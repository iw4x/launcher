#include <sstream>
#include <iomanip>
#include <cmath>

namespace launcher
{
  // progress_tracker_traits default implementations.
  //
  template <typename S>
  S progress_tracker_traits<S>::
  format_bytes (std::uint64_t n)
  {
    std::ostringstream o;

    // Select the appropriate unit (IEC standard).
    //
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

  template <typename S>
  S progress_tracker_traits<S>::
  format_speed (float bps)
  {
    std::ostringstream o;

    // We output 0 precision for bytes/sec to avoid clutter (e.g., "500 B/s"
    // instead of "500.0 B/s").
    //
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

  template <typename S>
  S progress_tracker_traits<S>::
  format_duration (int s)
  {
    std::ostringstream o;

    int h (s / 3600);
    int m ((s % 3600) / 60);
    int sec (s % 60);

    // If we have hours, show them. Otherwise just M:S.
    //
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

  template <typename S>
  S progress_tracker_traits<S>::
  format_bar (float p, bool ind, int w)
  {
    std::ostringstream o;
    o << "[";

    if (ind)
    {
      // Indeterminate state: render a fixed "bouncer" pattern.
      //
      // Ideally this would be animated based on time, but for a static
      // formatted string, this simple indicator is sufficient.
      //
      o << " <==> ";
      for (int i (5); i < w; ++i)
        o << " ";
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

  template <typename T>
  void basic_progress_tracker<T>::
  update (std::uint64_t n) noexcept
  {
    std::uint64_t t (current_time_us ());
    std::uint64_t t0 (last_update_time_.load (std::memory_order_relaxed));

    // Throttle updates.
    //
    // Recalculating speed on every chunk (e.g., 4KB) is wasteful and noisy.
    // We strictly enforce a minimum time interval.
    //
    std::uint64_t dt (t - t0);

    if (t0 != 0 && dt < traits_type::min_update_interval_ms * 1000)
      return;

    std::uint64_t n0 (last_bytes_.load (std::memory_order_relaxed));

    // Calculate instantaneous speed.
    //
    float inst (0.0f);
    if (t0 != 0 && dt > 0)
    {
      std::uint64_t dn (n > n0 ? n - n0 : 0);
      inst = static_cast<float> (dn) /
             (static_cast<float> (dt) / 1000000.0f);
    }

    // Update EWMA (Exponentially Weighted Moving Average).
    //
    float s0 (speed_.load (std::memory_order_relaxed));
    float s;

    if (s0 == 0.0f)
      s = inst;
    else
      s = traits_type::ewma_alpha * inst +
          (1.0f - traits_type::ewma_alpha) * s0;

    // Store state.
    //
    last_bytes_.store (n, std::memory_order_relaxed);
    last_update_time_.store (t, std::memory_order_relaxed);
    speed_.store (s, std::memory_order_relaxed);

    // Update sample buffer (for debugging or advanced stats).
    //
    std::size_t i (sample_index_.fetch_add (1, std::memory_order_relaxed) %
                   traits_type::sample_window_size);

    samples_[i].bytes.store (n, std::memory_order_relaxed);
    samples_[i].time_us.store (t, std::memory_order_relaxed);
  }

  template <typename T>
  void basic_progress_tracker<T>::
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

  template <typename T>
  typename T::string_type basic_progress_formatter<T>::
  format (const progress_snapshot& snapshot) const
  {
    return format (snapshot, 15);
  }

  template <typename T>
  typename T::string_type basic_progress_formatter<T>::
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
        o << traits_type::format_bar (s.progress_ratio (), i, w);
        break;
      }

    case progress_style::detailed:
      {
        // [===>   ] 45% 450 KB / 1.0 MB @ 100 KB/s ETA: 5s
        //
        bool i (s.total_bytes == 0);
        int pct (static_cast<int> (s.progress_ratio () * 100));

        o << traits_type::format_bar (s.progress_ratio (), i, w)
          << " " << std::setw (3) << pct << "% "
          << traits_type::format_bytes (s.current_bytes)
          << " / "
          << traits_type::format_bytes (s.total_bytes)
          << " @ " << traits_type::format_speed (s.speed);

        int eta (s.eta_seconds ());
        if (eta > 0)
          o << " ETA: " << traits_type::format_duration (eta);

        break;
      }

    case progress_style::dnf:
    default:
      {
        // [===>   ] 45% 100 KB/s 5s
        //
        bool i (s.total_bytes == 0);
        int pct (static_cast<int> (s.progress_ratio () * 100));

        o << traits_type::format_bar (s.progress_ratio (), i, w)
          << " " << std::setw (3) << pct << "% "
          << std::setw (10) << std::left
          << traits_type::format_speed (s.speed);

        int eta (s.eta_seconds ());
        if (eta > 0)
          o << " " << std::setw (8)
            << traits_type::format_duration (eta);

        break;
      }
    }

    return o.str ();
  }
}
