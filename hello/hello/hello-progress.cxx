#include <hello/hello-progress.hxx>

#include <hello/progress/progress-manager.hxx>
#include <hello/progress/progress-tracker.hxx>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace hello
{
  progress_coordinator::progress_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      manager_ (std::make_unique<manager_type> (ioc))
  {
  }

  void
  progress_coordinator::start ()
  {
    manager_->start ();
  }

  asio::awaitable<void>
  progress_coordinator::stop ()
  {
    co_await manager_->stop ();
  }

  bool
  progress_coordinator::running () const noexcept
  {
    return manager_->running ();
  }

  std::shared_ptr<progress_coordinator::entry_type>
  progress_coordinator::add_entry (std::string label)
  {
    return manager_->add_entry (std::move (label));
  }

  void
  progress_coordinator::remove_entry (std::shared_ptr<entry_type> entry)
  {
    manager_->remove_entry (std::move (entry));
  }

  void
  progress_coordinator::update_progress (std::shared_ptr<entry_type> entry,
                                         std::uint64_t current_bytes,
                                         std::uint64_t total_bytes)
  {
    entry->metrics ().current_bytes.store (current_bytes,
                                           std::memory_order_relaxed);

    entry->metrics ().total_bytes.store (total_bytes,
                                         std::memory_order_relaxed);

    progress_state state = (total_bytes > 0 && current_bytes >= total_bytes)
      ? progress_state::completed
      : progress_state::active;

    entry->metrics ().state.store (state, std::memory_order_relaxed);

    entry->tracker ().update (current_bytes);

    entry->metrics ().speed.store (entry->tracker ().speed (),
                                   std::memory_order_relaxed);
  }

  void
  progress_coordinator::set_status (std::string message)
  {
    manager_->set_status (std::move (message));
  }

  void
  progress_coordinator::add_log (std::string message)
  {
    manager_->add_log (std::move (message));
  }

  progress_coordinator::manager_type&
  progress_coordinator::manager () noexcept
  {
    return *manager_;
  }

  const progress_coordinator::manager_type&
  progress_coordinator::manager () const noexcept
  {
    return *manager_;
  }

  std::string
  format_progress (const progress_metrics& metrics)
  {
    std::ostringstream oss;

    std::uint64_t current (
      metrics.current_bytes.load (std::memory_order_relaxed));
    std::uint64_t total (metrics.total_bytes.load (std::memory_order_relaxed));
    float speed (metrics.speed.load (std::memory_order_relaxed));

    oss << format_bytes (current) << " / " << format_bytes (total);

    if (total > 0)
    {
      float ratio (static_cast<float> (current) / static_cast<float> (total));
      oss << " (" << std::fixed << std::setprecision (1) << (ratio * 100.0f)
          << "%)";
    }

    if (speed > 0.0f)
    {
      oss << " @ " << format_speed (speed);

      int eta (metrics.eta_seconds ());
      if (eta > 0)
        oss << ", ETA " << format_duration (eta);
    }

    return oss.str ();
  }

  std::string
  format_bytes (std::uint64_t bytes)
  {
    const char* units [] = {"B", "KiB", "MiB", "GiB", "TiB"};
    const std::size_t num_units (sizeof (units) / sizeof (units [0]));

    double value (static_cast<double> (bytes));
    std::size_t unit_index (0);

    while (value >= 1024.0 && unit_index < num_units - 1)
    {
      value /= 1024.0;
      ++unit_index;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision (1) << value << " "
        << units [unit_index];

    return oss.str ();
  }

  std::string
  format_speed (float bytes_per_sec)
  {
    return format_bytes (static_cast<std::uint64_t> (bytes_per_sec)) + "/s";
  }

  std::string
  format_duration (int seconds)
  {
    if (seconds < 60)
    {
      return std::to_string (seconds) + "s";
    }
    else if (seconds < 3600)
    {
      int minutes (seconds / 60);
      int secs (seconds % 60);

      std::ostringstream oss;
      oss << minutes << "m";

      if (secs > 0)
        oss << " " << secs << "s";

      return oss.str ();
    }
    else
    {
      int hours (seconds / 3600);
      int minutes ((seconds % 3600) / 60);

      std::ostringstream oss;
      oss << hours << "h";

      if (minutes > 0)
        oss << " " << minutes << "m";

      return oss.str ();
    }
  }

  std::string
  format_progress_bar (float progress_ratio, int width, bool indeterminate)
  {
    std::ostringstream oss;
    oss << "[";

    if (indeterminate)
    {
      for (int i (0); i < width; ++i)
        oss << (i == width / 2 ? '>' : ' ');
    }
    else
    {
      int filled (static_cast<int> (std::round (progress_ratio * width)));
      filled = std::min (filled, width);

      for (int i (0); i < width; ++i)
      {
        if (i < filled - 1)
          oss << '=';
        else if (i == filled - 1)
          oss << '>';
        else
          oss << ' ';
      }
    }

    oss << "]";

    return oss.str ();
  }
}
