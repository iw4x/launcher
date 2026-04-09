#pragma once

#include <launcher/progress/progress-types.hxx>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace launcher
{
  class progress_tracker
  {
  public:
    static constexpr float ewma_alpha = 0.2f;
    static constexpr std::chrono::milliseconds min_update_interval {500};
    static constexpr std::size_t sample_window_size = 10;

    // Formatting utilities
    static std::string format_bytes (std::uint64_t bytes);
    static std::string format_speed (float bytes_per_sec);
    static std::string format_duration (int seconds);
    static std::string format_bar (float progress, bool indeterminate, int width,
                                   unsigned frame = 0);

    progress_tracker () = default;

    progress_tracker (const progress_tracker&) = delete;
    progress_tracker& operator= (const progress_tracker&) = delete;

    progress_tracker (progress_tracker&&) noexcept = default;
    progress_tracker& operator= (progress_tracker&&) noexcept = default;

    void update (std::uint64_t current_bytes) noexcept;
    void reset () noexcept;

    float
    speed () const noexcept
    {
      return speed_.load (std::memory_order_relaxed);
    }

    std::string speed_string () const;

  private:
    std::atomic<std::uint64_t> last_bytes_ {0};
    std::atomic<std::uint64_t> last_update_time_ {0};
    std::atomic<float> speed_ {0.0f};

    struct sample
    {
      std::atomic<std::uint64_t> bytes {0};
      std::atomic<std::uint64_t> time_us {0};
    };

    std::array<sample, sample_window_size> samples_;
    std::atomic<std::size_t> sample_index_ {0};
  };

  class progress_formatter
  {
  public:
    explicit
    progress_formatter (progress_style style = progress_style::dnf)
      : style_ (style)
    {
    }

    std::string format (const progress_snapshot& snapshot) const;
    std::string format (const progress_snapshot& snapshot, int bar_width) const;

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
}
