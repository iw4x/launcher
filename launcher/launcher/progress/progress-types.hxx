// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <cstdint>
#include <cstddef>
#include <chrono>
#include <atomic>

namespace launcher
{
  enum class progress_state
  {
    idle,
    active,
    paused,
    completed,
    failed
  };

  // Text progress styles (see progress_formatter).
  //
  enum class progress_style
  {
    dnf,        // Bar, percentage, speed, and time left.
    simple,     // Percentage.
    bar,        // Bar.
    detailed    // Bar, percentage, sizes, speed, and time left.
  };

  // @@ Nothing uses this enumeration (progress_tracker always uses EWMA).
  //
  enum class speed_calculation
  {
    instant,    // Speed since the last update.
    average,    // Average speed since the start.
    ewma        // Exponentially weighted moving average.
  };

  using duration_type = std::chrono::milliseconds;
  using time_point = std::chrono::steady_clock::time_point;

  // Progress of a transfer, which the transfer updates while the progress
  // manager reads it.
  //
  // Note that moving is not atomic as a whole.
  //
  struct progress_metrics
  {
    std::atomic<std::uint64_t> total_bytes {0};
    std::atomic<std::uint64_t> current_bytes {0};
    std::atomic<std::uint64_t> completed_items {0};
    std::atomic<std::uint64_t> total_items {0};
    std::atomic<float> speed {0.0f};  // Bytes per second.
    std::atomic<progress_state> state {progress_state::idle};

    progress_metrics () = default;

    progress_metrics (const progress_metrics&) = delete;
    progress_metrics& operator= (const progress_metrics&) = delete;

    progress_metrics (progress_metrics&& other) noexcept
      : total_bytes (other.total_bytes.load ()),
        current_bytes (other.current_bytes.load ()),
        completed_items (other.completed_items.load ()),
        total_items (other.total_items.load ()),
        speed (other.speed.load ()),
        state (other.state.load ())
    {
    }

    progress_metrics& operator= (progress_metrics&& other) noexcept
    {
      if (this != &other)
      {
        total_bytes.store (other.total_bytes.load ());
        current_bytes.store (other.current_bytes.load ());
        completed_items.store (other.completed_items.load ());
        total_items.store (other.total_items.load ());
        speed.store (other.speed.load ());
        state.store (other.state.load ());
      }
      return *this;
    }

    // Return the ratio from 0 to 1 or 0 if the total is unknown.
    //
    float
    progress_ratio () const noexcept
    {
      std::uint64_t total (total_bytes.load (std::memory_order_relaxed));
      if (total == 0)
        return 0.0f;

      std::uint64_t current (current_bytes.load (std::memory_order_relaxed));
      return static_cast<float>(current) / static_cast<float>(total);
    }

    // Return the estimated number of seconds left or 0 if unknown.
    //
    int
    eta_seconds () const noexcept
    {
      float spd (speed.load (std::memory_order_relaxed));
      if (spd <= 0.0f)
        return 0;

      std::uint64_t total (total_bytes.load (std::memory_order_relaxed));
      std::uint64_t current (current_bytes.load (std::memory_order_relaxed));

      if (total <= current)
        return 0;

      return static_cast<int>((total - current) / spd);
    }
  };

  // Copy of the progress metrics at some point in time, for rendering.
  //
  // Note that the copy is not atomic as a whole, so the values can be from
  // slightly different times.
  //
  struct progress_snapshot
  {
    std::uint64_t total_bytes;
    std::uint64_t current_bytes;
    std::uint64_t completed_items;
    std::uint64_t total_items;
    float speed;
    progress_state state;
    time_point timestamp;

    progress_snapshot () = default;

    explicit
    progress_snapshot (const progress_metrics& m)
      : total_bytes (m.total_bytes.load (std::memory_order_relaxed)),
        current_bytes (m.current_bytes.load (std::memory_order_relaxed)),
        completed_items (m.completed_items.load (std::memory_order_relaxed)),
        total_items (m.total_items.load (std::memory_order_relaxed)),
        speed (m.speed.load (std::memory_order_relaxed)),
        state (m.state.load (std::memory_order_relaxed)),
        timestamp (std::chrono::steady_clock::now ())
    {
    }

    // As above.
    //
    float
    progress_ratio () const noexcept
    {
      if (total_bytes == 0)
        return 0.0f;
      return static_cast<float>(current_bytes) / static_cast<float>(total_bytes);
    }

    int
    eta_seconds () const noexcept
    {
      if (speed <= 0.0f)
        return 0;

      if (total_bytes <= current_bytes)
        return 0;

      return static_cast<int>((total_bytes - current_bytes) / speed);
    }
  };
}
