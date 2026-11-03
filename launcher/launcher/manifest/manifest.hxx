// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <launcher/manifest/manifest-types.hxx>

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <utility>
#include <ostream>
#include <stdexcept>

#include <boost/asio/awaitable.hpp>
#include <boost/json/value.hpp>

namespace launcher
{
  namespace asio = boost::asio;
  namespace json = boost::json;

  // Hash value in hex, as in the manifest.
  //
  struct hash
  {
    hash_algorithm algorithm;
    std::string value;

    hash () : algorithm (hash_algorithm::blake3) {}

    hash (hash_algorithm a, std::string v)
      : algorithm (a), value (std::move (v)) {}

    explicit hash (std::string v)
      : algorithm (hash_algorithm::blake3), value (std::move (v)) {}

    bool
    operator== (const hash&) const = default;

    bool
    empty () const noexcept
    {
      return value.empty ();
    }

    std::string
    string () const;
  };

  // File entry in a manifest.
  //
  struct manifest_file
  {
    using size_type = std::uint64_t;

    launcher::hash hash;
    size_type size;
    std::string path;                        // Relative to the installation.
    std::optional<std::string> asset_name;
    std::optional<std::string> archive_name; // Archive that contains it.

    manifest_file () : size (0) {}

    manifest_file (launcher::hash h,
                   size_type s,
                   std::string p,
                   std::optional<std::string> a = std::nullopt,
                   std::optional<std::string> ar = std::nullopt)
      : hash (std::move (h)),
        size (s),
        path (std::move (p)),
        asset_name (std::move (a)),
        archive_name (std::move (ar)) {}

    bool
    operator== (const manifest_file&) const = default;

    bool
    empty () const noexcept
    {
      return path.empty ();
    }
  };

  // Archive entry in a manifest.
  //
  struct manifest_archive
  {
    using size_type = std::uint64_t;

    launcher::hash hash;
    size_type size;
    std::string name;
    std::string url;
    compression_type compression;
    std::vector<manifest_file> files;

    // Paths of the files in the archive that we don't install.
    //
    std::vector<std::string> exclude;

    // Files to install under another name, as pairs of the path in the
    // archive and the path to install it as.
    //
    std::vector<std::pair<std::string, std::string>> rename;

    manifest_archive ()
      : size (0), compression (compression_type::none) {}

    manifest_archive (launcher::hash h,
                      size_type s,
                      std::string n,
                      std::string u = std::string (),
                      compression_type c = compression_type::none)
      : hash (std::move (h)),
        size (s),
        name (std::move (n)),
        url (std::move (u)),
        compression (c) {}

    // Note that the comparison ignores the files, exclusions, and renames.
    //
    bool
    operator== (const manifest_archive& o) const noexcept
    {
      return hash == o.hash &&
             size == o.size &&
             name == o.name &&
             url == o.url &&
             compression == o.compression;
    }

    bool
    empty () const noexcept
    {
      return name.empty ();
    }
  };

  // Manifest of the files to install, either individually or from
  // archives, in the JSON format:
  //
  // {
  //   "archives": [{"name": ..., "url": ..., "size": ..., "blake3": ...}],
  //   "files": [{"path": ..., "size": ..., "blake3": ...,
  //              "asset_name": ..., "archive": ...}]
  // }
  //
  // Where a DLC manifest has no archives and its files no archive. Note that
  // parsing skips entries of the wrong type as well as files without a path
  // and archives without a name.
  //
  class manifest
  {
  public:
    manifest_format kind;
    std::vector<manifest_archive> archives;
    std::vector<manifest_file> files;

    manifest () : kind (manifest_format::update) {}

    // Parse the manifest. Throw std::runtime_error if it is not valid JSON
    // or not an object.
    //
    explicit
    manifest (const std::string& json_str,
              manifest_format k = manifest_format::update);

    explicit
    manifest (const json::value& jv,
              manifest_format k = manifest_format::update);

    bool
    operator== (const manifest&) const = default;

    bool
    empty () const noexcept
    {
      return archives.empty () && files.empty ();
    }

    std::string
    string () const;

    json::value
    json () const;

    // Copy each file with an archive name into the files of that archive.
    //
    void
    link_files ();

    // @@ Always returns true.
    //
    bool
    validate () const;

    // As the constructor and validate() but as coroutines. Note that they
    // don't suspend.
    //
    static asio::awaitable<manifest>
    parse_async (const std::string& json_str,
                 manifest_format k = manifest_format::update);

    asio::awaitable<bool>
    validate_async () const;

  private:
    void
    parse_update (const json::object&);

    void
    parse_dlc (const json::object&);

    json::object
    serialize_update () const;

    json::object
    serialize_dlc () const;
  };

  inline std::ostream&
  operator<< (std::ostream& o, const hash& h)
  {
    return o << h.string ();
  }

  inline std::ostream&
  operator<< (std::ostream& o, const manifest& m)
  {
    return o << m.string ();
  }
}
