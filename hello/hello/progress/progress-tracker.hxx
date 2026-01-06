#pragma once

#include <hello/progress/progress-types.hxx>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <algorithm>

namespace hello
{
  // Traits for progress tracking customization.
  //
  template <typename S = std::string>
  struct progress_tracker_traits
  {
    using string_type = S;

    // EWMA alpha factor for speed calculation (0.0-1.0).
    // Higher = more weight on recent samples.
    // Lower value (0.2) makes speed display more stable like DNF.
    //
    static constexpr float ewma_alpha = 0.2f;

    // Minimum update interval in milliseconds.
    // Larger interval (500ms) makes speed display more stable.
    //
    static constexpr int min_update_interval_ms = 500;

    // Sample window size for average calculation.
    //
    static constexpr std::size_t sample_window_size = 10;

    // Format bytes to human-readable string.
    //
    static string_type
    format_bytes (std::uint64_t bytes);

    // Format speed to human-readable string.
    //
    static string_type
    format_speed (float bytes_per_sec);

    // Format duration to human-readable string.
    //
    static string_type
    format_duration (int seconds);

    // Format progress bar.
    //
    static string_type
    format_bar (float progress, bool indeterminate, int width);
  };

  // Lock-free speed tracker using EWMA or sliding window.
  //
  template <typename T = progress_tracker_traits<>>
  class basic_progress_tracker
  {
  public:
    using traits_type = T;
    using string_type = typename traits_type::string_type;

    basic_progress_tracker () = default;

    // Non-copyable but movable.
    //
    basic_progress_tracker (const basic_progress_tracker&) = delete;
    basic_progress_tracker& operator= (const basic_progress_tracker&) = delete;

    basic_progress_tracker (basic_progress_tracker&&) noexcept = default;
    basic_progress_tracker& operator= (basic_progress_tracker&&) noexcept = default;

    // Update with new byte count (lock-free).
    //
    void
    update (std::uint64_t current_bytes) noexcept;

    // Get current speed in bytes/sec (lock-free read).
    //
    float
    speed () const noexcept
    {
      return speed_.load (std::memory_order_relaxed);
    }

    // Reset tracker.
    //
    void
    reset () noexcept;

    // Get formatted speed string.
    //
    string_type
    speed_string () const
    {
      return traits_type::format_speed (speed ());
    }

  private:
    std::atomic<std::uint64_t> last_bytes_ {0};
    std::atomic<std::uint64_t> last_update_time_ {0};  // Microseconds since epoch
    std::atomic<float> speed_ {0.0f};

    // Sample ring buffer for sliding window average.
    //
    struct sample
    {
      std::atomic<std::uint64_t> bytes {0};
      std::atomic<std::uint64_t> time_us {0};
    };

    std::array<sample, traits_type::sample_window_size> samples_;
    std::atomic<std::size_t> sample_index_ {0};
  };

  // Progress formatter with various styles.
  //
  template <typename T = progress_tracker_traits<>>
  class basic_progress_formatter
  {
  public:
    using traits_type = T;
    using string_type = typename traits_type::string_type;

    explicit
    basic_progress_formatter (progress_style style = progress_style::dnf)
      : style_ (style)
    {
    }

    // Format progress snapshot to string.
    //
    string_type
    format (const progress_snapshot& snapshot) const;

    // Format with custom width for progress bar.
    //
    string_type
    format (const progress_snapshot& snapshot, int bar_width) const;

    // Set display style.
    //
    void
    set_style (progress_style style) noexcept
    {
      style_ = style;
    }

    progress_style
    style () const noexcept
    {
      return style_;
    }

  private:
    progress_style style_;
  };

  using progress_tracker = basic_progress_tracker<>;
  using progress_formatter = basic_progress_formatter<>;
}

#include <hello/progress/progress-tracker.ixx>
#include <hello/progress/progress-tracker.txx>
