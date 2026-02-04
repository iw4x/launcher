#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <ostream>
#include <string>

namespace launcher
{
  // Build2 standard version for launcher releases.
  //
  // Format: <major>.<minor>.<patch>[-(a|b).<num>[.z|<snapsn>[.<snapid>]]]
  //
  // Examples:
  //   1.1.0                                 - final release
  //   1.2.0-a.1                             - first alpha pre-release
  //   1.2.0-b.2                             - second beta pre-release
  //   1.2.0-a.1.z                           - alpha development snapshot
  //   1.2.0-a.1.20260201010251.fe4660334ed0 - alpha snapshot
  //
  struct launcher_version
  {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;

    // Pre-release: 0 = release, 1-499 = alpha, 500-999 = beta. The actual
    // number is stored, e.g., alpha.2 = 2, beta.3 = 503.
    //
    std::uint16_t pre_release = 0;

    // Snapshot sequence number (0 = not a snapshot). For git, this is the
    // commit timestamp in YYYYMMDDhhmmss form.
    //
    std::uint64_t snapshot_sn = 0;

    // Snapshot id (empty if not specified). For git, this is the abbreviated
    // commit id.
    //
    std::string snapshot_id;

    launcher_version () = default;

    launcher_version (std::uint32_t mj,
                      std::uint32_t mi,
                      std::uint32_t pa,
                      std::uint16_t pr = 0,
                      std::uint64_t sn = 0,
                      std::string   si = "")
      : major (mj),
        minor (mi),
        patch (pa),
        pre_release (pr),
        snapshot_sn (sn),
        snapshot_id (std::move (si))
    {
    }

    bool
    empty () const noexcept
    {
      return major == 0 && minor == 0 && patch == 0;
    }

    bool
    release () const noexcept
    {
      return pre_release == 0 && snapshot_sn == 0;
    }

    bool
    is_alpha () const noexcept
    {
      return pre_release > 0 && pre_release < 500;
    }

    bool
    is_beta () const noexcept
    {
      return pre_release >= 500;
    }

    bool
    snapshot () const noexcept
    {
      return snapshot_sn != 0;
    }

    std::optional<std::uint16_t>
    alpha () const noexcept
    {
      if (is_alpha ())
        return pre_release;
      return std::nullopt;
    }

    std::optional<std::uint16_t>
    beta () const noexcept
    {
      if (is_beta ())
        return static_cast<std::uint16_t> (pre_release - 500);
      return std::nullopt;
    }

    // Compare versions. Returns negative if this < other, positive if this >
    // other, zero if equal.
    //
    int
    compare (const launcher_version& v) const noexcept;

    // String representation (e.g., "1.2.0-a.1").
    //
    std::string
    string () const;
  };

  inline bool
  operator< (const launcher_version& x, const launcher_version& y)
  {
    return x.compare (y) < 0;
  }

  inline bool
  operator> (const launcher_version& x, const launcher_version& y)
  {
    return x.compare (y) > 0;
  }

  inline bool
  operator== (const launcher_version& x, const launcher_version& y)
  {
    return x.compare (y) == 0;
  }

  inline bool
  operator!= (const launcher_version& x, const launcher_version& y)
  {
    return !(x == y);
  }

  inline bool
  operator<= (const launcher_version& x, const launcher_version& y)
  {
    return x.compare (y) <= 0;
  }

  inline bool
  operator>= (const launcher_version& x, const launcher_version& y)
  {
    return x.compare (y) >= 0;
  }

  inline std::ostream&
  operator<< (std::ostream& os, const launcher_version& v)
  {
    return os << v.string ();
  }

  // Parse version string. Returns nullopt if parsing fails.
  //
  // The input may optionally have a 'v' prefix (e.g., "v1.2.0").
  //
  std::optional<launcher_version>
  parse_launcher_version (const std::string& s);

  // Update check result.
  //
  enum class update_status
  {
    up_to_date,
    update_available,
    check_failed
  };

  inline std::ostream&
  operator<< (std::ostream& os, update_status s)
  {
    switch (s)
    {
      case update_status::up_to_date:       return os << "up_to_date";
      case update_status::update_available: return os << "update_available";
      case update_status::check_failed:     return os << "check_failed";
    }
    return os;
  }

  // Update state enumeration.
  //
  enum class update_state
  {
    idle,
    checking,
    downloading,
    verifying,
    installing,
    restarting,
    completed,
    failed
  };

  inline std::ostream&
  operator<< (std::ostream& os, update_state s)
  {
    switch (s)
    {
      case update_state::idle:        return os << "idle";
      case update_state::checking:    return os << "checking";
      case update_state::downloading: return os << "downloading";
      case update_state::verifying:   return os << "verifying";
      case update_state::installing:  return os << "installing";
      case update_state::restarting:  return os << "restarting";
      case update_state::completed:   return os << "completed";
      case update_state::failed:      return os << "failed";
    }
    return os;
  }

  // Platform identification for asset selection.
  //
  enum class platform_type
  {
    windows_x64,
    linux_x64,
    unknown
  };

  inline std::ostream&
  operator<< (std::ostream& os, platform_type p)
  {
    switch (p)
    {
      case platform_type::windows_x64: return os << "x86_64-windows";
      case platform_type::linux_x64:   return os << "x86_64-linux-glibc";
      case platform_type::unknown:     return os << "unknown";
    }
    return os;
  }

  // Get the current platform.
  //
  platform_type
  current_platform () noexcept;

  // Update information from a GitHub release.
  //
  struct update_info
  {
    launcher_version version;
    std::string tag_name;
    std::string release_url;
    std::string asset_url;
    std::string asset_name;
    std::uint64_t asset_size = 0;
    bool prerelease = false;
    std::string body;  // release notes (markdown)

    bool
    empty () const noexcept
    {
      return version.empty ();
    }
  };

  // Update progress callback.
  //
  // Called during update with current state and progress (0.0-1.0).
  //
  using update_progress_callback =
    std::function<void (update_state state,
                        double progress,
                        const std::string& message)>;
}
