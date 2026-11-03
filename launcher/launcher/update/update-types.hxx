// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <ostream>
#include <string>

namespace launcher
{
  // Launcher release version, which is the part of the standard version
  // that our releases use:
  //
  // <major>.<minor>.<patch>[-(a|b).<num>[.(z|<snapsn>[.<snapid>])]]
  //
  // For example:
  //
  // 1.1.0                                  Final release.
  // 1.2.0-a.1                              First alpha.
  // 1.2.0-b.2                              Second beta.
  // 1.2.0-a.1.z                            Development snapshot after a.1.
  // 1.2.0-a.1.20260201010251.fe4660334ed0  Snapshot after a.1.
  //
  struct launcher_version
  {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;

    // Pre-release number with betas offset by 500, so that the integer
    // order is the version order. For example, a.2 is 2 and b.3 is 503. A
    // final release has 0.
    //
    std::uint16_t pre_release = 0;

    // Snapshot sequence number or 0 if this is not a snapshot. For git it is
    // the commit time (YYYYMMDDhhmmss) and for the .z placeholder it is 1.
    //
    std::uint64_t snapshot_sn = 0;

    // Snapshot id, which for git is the abbreviated commit id. Can be empty.
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

    // Return true if this is 0.0.0, which update_info uses to mean no
    // update.
    //
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

    // Return a negative value if this version is less than v, 0 if they are
    // equal, and a positive value otherwise.
    //
    int
    compare (const launcher_version& v) const noexcept;

    // Return the version in the standard form (e.g., 1.2.0-a.1).
    //
    // @@ The .z placeholder comes back as the .1 snapshot.
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

  // Parse the version, which may have the v prefix of git tags (e.g.,
  // v1.2.0). Return nullopt if it is invalid.
  //
  // Note that we ignore what follows the patch number unless it starts with
  // '-', so 1.2.0x is 1.2.0.
  //
  std::optional<launcher_version>
  parse_launcher_version (const std::string& s);

  // Outcome of an update check.
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

  // Update step, for progress reporting.
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

  // Platform of a launcher release asset. It prints as the platform part of
  // the asset name.
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

  // Return the platform that we are built for.
  //
  platform_type
  current_platform () noexcept;

  // Launcher release that we can update to and its asset for the current
  // platform. Empty if there is none.
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
    std::string body;  // Release notes (Markdown).

    bool
    empty () const noexcept
    {
      return version.empty ();
    }
  };

  // Progress callback. The progress of the step goes from 0.0 to 1.0.
  //
  using update_progress_callback =
    std::function<void (update_state state,
                        double progress,
                        const std::string& message)>;
}
