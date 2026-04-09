#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <odb/core.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // File state as discovered on disk. Note that we don't differentiate between
  // corrupted and missing at this stage. Unknown files are mostly left over
  // from interrupted updates or manual meddling.
  //
  enum class file_state
  {
    valid,   // Mtime matches our cache.
    stale,   // File exists but mtime is different.
    missing, // File is gone entirely.
    unknown  // We have never seen this file before.
  };

  std::ostream&
  operator<< (std::ostream& o, file_state s);

  // Logical component groupings. We use these to perform partial verifications
  // or to skip checking massive rawfiles when we are just updating the launcher
  // itself.
  //
  enum class component_type
  {
    client,   // Main release artifacts.
    rawfiles, // Content data.
    dlc,      // Zone files (external).
    helper,   // Platform-specific helpers (e.g., Steam integration).
    launcher  // Our own executable.
  };

  std::ostream&
  operator<< (std::ostream& o, component_type c);

  // Action to take on a specific file during reconciliation. Note that the
  // verify action implies we will hash the file first. If the hash mismatches,
  // the action seamlessly demotes to download.
  //
  enum class reconcile_action
  {
    none,     // Everything is fine.
    download, // Fetch it from the mirror.
    verify,   // Something looks off, hash it.
    remove    // It shouldn't be here.
  };

  std::ostream&
  operator<< (std::ostream& o, reconcile_action a);

  // Persistent metadata for files we have downloaded.
  //
  #pragma db object table("cached_files")
  class cached_file
  {
  public:
    cached_file () = default;

    cached_file (std::string p,
                 std::int64_t t,
                 std::string v,
                 component_type c,
                 std::uint64_t s = 0,
                 std::string h = std::string ())
      : path_ (std::move (p)),
        mtime_ (t),
        version_ (std::move (v)),
        component_ (c),
        size_ (s),
        hash_ (std::move (h))
    {
    }

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

    void
    set_mtime (std::int64_t t) { mtime_ = t; }

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
    std::string hash_;
  };

  // Tracks the currently installed version tag for each component. Note that a
  // component might technically be out of sync with this tag if a download was
  // interrupted. This is why we rely on individual file hashes for the actual
  // reconciliation.
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

  // Generic key-value store. We keep it untyped here since the higher layers
  // know how to parse strings into bools or paths.
  //
  #pragma db object table("user_settings")
  class user_setting
  {
  public:
    user_setting () = default;

    user_setting (std::string k, std::string v)
      : key_ (std::move (k)),
        val_ (std::move (v))
    {
    }

    const std::string&
    key () const noexcept { return key_; }

    const std::string&
    val () const noexcept { return val_; }

    void
    val (std::string v) { val_ = std::move (v); }

  private:
    friend class odb::access;

    #pragma db id
    std::string key_;
    std::string val_;
  };

  // Blake3 hash digest.
  //
  // An empty blake3_hash signals "no hash available" and is explicitly
  // supported. Every non-empty hash must be exactly 64 hex characters
  // (256-bit Blake3 output).
  //
  class blake3_hash
  {
  public:
    blake3_hash () = default;

    explicit
    blake3_hash (std::string hex);

    explicit
    blake3_hash (std::string_view hex)
      : blake3_hash (std::string (hex)) {}

    bool
    empty () const noexcept
    {
      return hex_.empty ();
    }

    const std::string&
    string () const noexcept
    {
      return hex_;
    }

    bool
    verify_file (const fs::path& p) const;

    // Compute the Blake3 hash of a file and return the result. Returns an
    // empty hash on I/O failure.
    //
    static blake3_hash
    of_file (const fs::path& p);

    bool
    operator == (const blake3_hash& o) const noexcept
    {
      return hex_ == o.hex_;
    }

    bool
    operator != (const blake3_hash& o) const noexcept
    {
      return hex_ != o.hex_;
    }

  private:
    std::string hex_;
  };

  std::ostream&
  operator<< (std::ostream& o, const blake3_hash& h);

  // Record of what we need to do to a given file. Note that we default to the
  // client component here. It is the most common case and saves us from
  // sprinkling explicit initializers in the discovery loop.
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

  // Aggregated totals used primarily by the UI to render progress and status.
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
      // We consider the state fully synchronized only if there is nothing to
      // fetch and we don't have dangling references or stale files.
      //
      return downloads_required == 0 &&
             files_stale == 0 &&
             files_missing == 0;
    }
  };

  // Return mtime as seconds since epoch. We use int64_t instead of std::time_t
  // to guarantee a 64-bit width regardless of the platform.
  //
  std::int64_t
  get_file_mtime (const fs::path& p);

  std::int64_t
  current_timestamp ();

  // Blake3 is fast enough that we can usually compute it for the entire file in
  // one go.
  //
  std::string
  compute_blake3 (const fs::path& p);

  bool
  verify_blake3 (const fs::path& p, const std::string& h);
}
