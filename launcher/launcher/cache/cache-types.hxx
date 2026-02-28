#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <odb/core.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // We need to categorize the state of a file on disk relative to what we
  // expect from our cache to decide how to reconcile it.
  //
  enum class file_state
  {
    valid,    // Mtime matches our cache.
    stale,    // File exists but mtime is different.
    missing,  // File is gone.
    unknown   // We have never seen this file before. ඞ
  };

  inline std::ostream&
  operator<< (std::ostream& os, file_state s)
  {
    switch (s)
    {
      case file_state::valid:   return os << "valid";
      case file_state::stale:   return os << "stale";
      case file_state::missing: return os << "missing";
      case file_state::unknown: return os << "unknown";
    }
    return os;
  }

  // Distinct components have different update rules. For instance, we might
  // enforce stricter checks on the client binaries than on bulk DLC assets.
  //
  enum class component_type
  {
    client,   // Main release artifacts.
    rawfiles, // Content data.
    dlc,      // Zone files (external).
    helper,   // Platform-specific helpers (e.g., Steam integration).
    launcher  // Our own executable.
  };

  inline std::ostream&
  operator<< (std::ostream& os, component_type c)
  {
    switch (c)
    {
      case component_type::client:   return os << "client";
      case component_type::rawfiles: return os << "rawfiles";
      case component_type::dlc:      return os << "dlc";
      case component_type::helper:   return os << "helper";
      case component_type::launcher: return os << "launcher";
    }
    return os;
  }

  // The decision on what to do with a file after we've inspected its state.
  //
  enum class reconcile_action
  {
    none,     // Everything is fine.
    download, // Fetch it from the mirror.
    verify,   // Something looks off, hash it.
    remove    // It shouldn't be here.
  };

  inline std::ostream&
  operator<< (std::ostream& os, reconcile_action a)
  {
    switch (a)
    {
      case reconcile_action::none:     return os << "none";
      case reconcile_action::download: return os << "download";
      case reconcile_action::verify:   return os << "verify";
      case reconcile_action::remove:   return os << "remove";
    }
    return os;
  }

  // Metadata to persist to the database.
  //
  // We rely on mtime for the fast path (similar to build systems). If the
  // mtime matches, we assume the file is the one we verified previously.
  //
  #pragma db object table("cached_files")
  class cached_file
  {
  public:
    cached_file () = default;

    cached_file (std::string p,
                 std::int64_t mt,
                 std::string v,
                 component_type c,
                 std::uint64_t s = 0,
                 std::string h = "")
      : path_ (std::move (p)),
        mtime_ (mt),
        version_ (std::move (v)),
        component_ (c),
        size_ (s),
        hash_ (std::move (h))
    {
    }

    // Accessors.
    //
    const std::string&
    path () const noexcept { return path_; }

    std::int64_t
    mtime () const noexcept { return mtime_; }

    const std::string&
    version () const noexcept { return version_; }

    component_type
    component () const noexcept { return component_; }

    std::uint64_t
    size () const noexcept { return size_; }

    const std::string&
    hash () const noexcept { return hash_; }

    // Mutators.
    //
    void
    set_mtime (std::int64_t mt) { mtime_ = mt; }

    void
    set_version (std::string v) { version_ = std::move (v); }

    void
    set_size (std::uint64_t s) { size_ = s; }

    void
    set_hash (std::string h) { hash_ = std::move (h); }

  private:
    friend class odb::access;

    #pragma db id
    std::string path_;

    #pragma db not_null
    std::int64_t mtime_;

    #pragma db not_null index
    std::string version_;

    #pragma db not_null
    component_type component_;

    std::uint64_t size_;

    // BLAKE3 hex string. Kept empty until we actually verify the file.
    //
    std::string hash_;
  };

  // We track the currently installed version tag for each component group.
  //
  // That is, we want to detect an update (e.g., "v1" -> "v2") without
  // scanning every single file first.
  //
  #pragma db object table("component_versions")
  class component_version
  {
  public:
    component_version () = default;

    component_version (component_type c,
                       std::string t,
                       std::int64_t ts)
      : component_ (c),
        tag_ (std::move (t)),
        installed_at_ (ts)
    {
    }

    component_type
    component () const noexcept { return component_; }

    const std::string&
    tag () const noexcept { return tag_; }

    std::int64_t
    installed_at () const noexcept { return installed_at_; }

    void
    set_tag (std::string t) { tag_ = std::move (t); }

    void
    set_installed_at (std::int64_t ts) { installed_at_ = ts; }

  private:
    friend class odb::access;

    #pragma db id
    component_type component_;

    #pragma db not_null
    std::string tag_;

    std::int64_t installed_at_;
  };

  // Refer to the legacy cache implementation for context.
  //

  // A transient unit of work for the reconciler.
  //
  struct reconcile_item
  {
    reconcile_action action;
    std::string path;
    std::string url;
    std::string expected_hash;
    std::uint64_t expected_size;
    component_type component;
    std::string version;

    reconcile_item ()
      : action (reconcile_action::none),
        expected_size (0),
        component (component_type::client)
    {
    }

    reconcile_item (reconcile_action a,
                    std::string p,
                    std::string u,
                    std::string h,
                    std::uint64_t s,
                    component_type c,
                    std::string v)
      : action (a),
        path (std::move (p)),
        url (std::move (u)),
        expected_hash (std::move (h)),
        expected_size (s),
        component (c),
        version (std::move (v))
    {
    }

    bool
    empty () const noexcept
    {
      return action == reconcile_action::none && path.empty ();
    }
  };

  // High-level stats to show the user what's happening.
  //
  struct reconcile_summary
  {
    std::size_t files_valid;
    std::size_t files_stale;
    std::size_t files_missing;
    std::size_t files_unknown;
    std::size_t downloads_required;
    std::uint64_t bytes_to_download;

    reconcile_summary ()
      : files_valid (0),
        files_stale (0),
        files_missing (0),
        files_unknown (0),
        downloads_required (0),
        bytes_to_download (0)
    {
    }

    bool
    up_to_date () const noexcept
    {
      return downloads_required == 0 &&
             files_stale == 0 &&
             files_missing == 0;
    }
  };

  // Get the modification time.
  //
  // We use the raw file_time_type representation because converting to
  // system_clock is messy and not strictly portable across C++ standard
  // libraries. As long as we are consistent in how we read it, the raw value
  // is fine.
  //
  inline std::int64_t
  get_file_mtime (const fs::path& p)
  {
    auto t (fs::last_write_time (p));
    return t.time_since_epoch ().count ();
  }

  // Simple epoch wrapper.
  //
  inline std::int64_t
  current_timestamp ()
  {
    auto now (std::chrono::system_clock::now ());
    auto e (now.time_since_epoch ());
    return std::chrono::duration_cast<std::chrono::seconds> (e).count ();
  }

  // Compute the BLAKE3 hash. Returns empty string on failure.
  //
  std::string
  compute_blake3 (const fs::path& p);

  // Check if the file on disk matches the expected hash.
  //
  inline bool
  verify_blake3 (const fs::path& p, const std::string& h)
  {
    if (h.empty ())
      return false;

    std::string a (compute_blake3 (p));
    return !a.empty () && a == h;
  }
}
