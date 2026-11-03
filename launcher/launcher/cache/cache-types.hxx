// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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

  // State of an installed file relative to its cache record (see
  // reconciler::stat()). Note that we only compare the file metadata, so a
  // stale file is not necessarily corrupt, it is just no longer known to be
  // intact.
  //
  enum class file_state
  {
    valid,   // Metadata matches the record.
    stale,   // Exists but its metadata differs from the record.
    missing, // Has a record but no longer exists.
    unknown  // Has no record.
  };

  std::ostream&
  operator<< (std::ostream& o, file_state s);

  // Installation component that a file belongs to. We track the version of
  // each component separately, which lets us verify or update one of them
  // without touching the rest (the rawfiles, for example, are large).
  //
  enum class component_type
  {
    client,     // IW4x client release.
    rawfiles,   // IW4x game data.
    dlc,        // DLC zone files.
    helper,     // Platform-specific helpers (Steam integration, etc).
    launcher,   // The launcher itself.
    client_x64, // IW4x (mm) client release.
    assets_x64  // IW4x (mm) game data.
  };

  std::ostream&
  operator<< (std::ostream& o, component_type c);

  // Action that reconciliation plans for a file. Note that a file to verify
  // is hashed first and only downloaded if the hash doesn't match.
  //
  enum class reconcile_action
  {
    none,     // Up to date.
    download, // Download from the release.
    verify,   // Hash and download if it doesn't match.
    remove    // Remove from the installation.
  };

  std::ostream&
  operator<< (std::ostream& o, reconcile_action a);

  // Cache record of an installed file. The path is the normalized absolute
  // path of the file (see reconciler::key()) and the modification time is
  // the one the file had when we recorded it (see get_file_mtime()).
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

  // Installed release tag of a component and when we installed it. Note
  // that an interrupted update can leave the files of a component out of
  // sync with its tag, which is why reconciliation goes by the records of
  // the individual files.
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

  // Setting stored in the cache as a key and its value. The values are
  // untyped and it is up to the user of a setting to interpret its value.
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

  // BLAKE3 digest in its hex representation.
  //
  // The digest is either empty, which stands for no digest, or the 64 hex
  // digits of a 256-bit BLAKE3 output, normalized to lower case.
  //
  class blake3_hash
  {
  public:
    blake3_hash () = default;

    // Create the digest from its hex representation, ignoring whitespaces
    // and case. Throw std::invalid_argument if the result is neither empty
    // nor 64 hex digits.
    //
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

    // Return true if the digest is not empty and matches the file. Return
    // false if the file cannot be read.
    //
    bool
    verify_file (const fs::path& p) const;

    // Return the digest of the file or an empty digest if the file cannot be
    // read.
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

  // Planned action for a file together with what we know about its expected
  // content. Note that the default-constructed item belongs to the client
  // component, the most common case.
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

  // Totals of a reconciliation plan, which we report to the user.
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

    // Return true if there is nothing to download and no file is stale or
    // missing.
    //
    bool
    up_to_date () const noexcept
    {
      return downloads_required == 0 &&
             files_stale == 0 &&
             files_missing == 0;
    }
  };

  // Return the modification time of the file as a count of file clock
  // ticks. Note that we only compare these values with each other, so their
  // epoch and resolution don't matter. Throw std::filesystem::filesystem_error
  // if the file cannot be examined.
  //
  std::int64_t
  get_file_mtime (const fs::path& p);

  // Return the current time in seconds since the UNIX epoch.
  //
  std::int64_t
  current_timestamp ();

  // Return the BLAKE3 digest of the file as 64 lower-case hex digits or an
  // empty string if the file cannot be read.
  //
  std::string
  compute_blake3 (const fs::path& p);

  // Return true if the digest of the file matches the expected one, ignoring
  // whitespaces and case. Return false if the expected digest is empty or the
  // file cannot be read.
  //
  bool
  verify_blake3 (const fs::path& p, const std::string& h);
}
