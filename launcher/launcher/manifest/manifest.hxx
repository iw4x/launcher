#pragma once

#include <launcher/manifest/manifest-types.hxx>

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>
#include <utility>
#include <ostream>
#include <stdexcept>

#include <boost/asio/awaitable.hpp>
#include <boost/json/value.hpp>

namespace launcher
{
  // Forward declarations for async operations.
  //
  namespace asio = boost::asio;
  namespace json = boost::json;

  // Hash value with algorithm type.
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
    empty () const noexcept
    {
      return value.empty ();
    }

    // String representation.
    //
    std::string
    string () const;

    // Verify hash against data.
    //
    bool
    verify (const std::string& data) const;

    bool
    verify (const std::vector<char>& data) const;
  };

  // File entry in a manifest.
  //
  struct manifest_file
  {
    using size_type = std::uint64_t;

    launcher::hash hash;
    size_type size;
    std::string path;
    std::optional<std::string> asset_name;
    std::optional<std::string> archive_name;

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

    bool
    empty () const noexcept
    {
      return name.empty ();
    }
  };

  // Main manifest class.
  //
  class manifest
  {
  public:
    manifest_format format;
    manifest_format kind;
    std::vector<manifest_archive> archives;
    std::vector<manifest_file> files;

    // Constructors.
    //
    manifest () : kind (manifest_format::update) {}

    explicit
    manifest (manifest_format f, manifest_format k = manifest_format::update)
      : format (f), kind (k) {}

    // Parse from JSON string.
    //
    explicit
    manifest (const std::string& json_str,
              manifest_format k = manifest_format::update);

    // Parse from JSON value.
    //
    explicit
    manifest (const json::value& jv,
              manifest_format k = manifest_format::update);

    // Empty check.
    //
    bool
    empty () const noexcept
    {
      return archives.empty () && files.empty ();
    }

    // Serialize to JSON string.
    //
    std::string
    string () const;

    // Serialize to JSON value.
    //
    json::value
    json () const;

    // Link files to their archives.
    //
    void
    link_files ();

    // Validate manifest integrity.
    //
    bool
    validate () const;

    // Async parse from JSON string with coroutine.
    //
    static asio::awaitable<manifest>
    parse_async (const std::string& json_str,
                 manifest_format k = manifest_format::update);

    // Async validate with coroutine.
    //
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

  // Comparison operators.
  //
  inline bool
  operator== (const hash& x, const hash& y) noexcept
  {
    return x.algorithm == y.algorithm && x.value == y.value;
  }

  inline bool
  operator!= (const hash& x, const hash& y) noexcept
  {
    return !(x == y);
  }

  inline bool
  operator== (const manifest_file& x, const manifest_file& y) noexcept
  {
    return x.hash == y.hash &&
           x.size == y.size &&
           x.path == y.path &&
           x.asset_name == y.asset_name &&
           x.archive_name == y.archive_name;
  }

  inline bool
  operator!= (const manifest_file& x, const manifest_file& y) noexcept
  {
    return !(x == y);
  }

  inline bool
  operator== (const manifest_archive& x, const manifest_archive& y) noexcept
  {
    return x.hash == y.hash &&
           x.size == y.size &&
           x.name == y.name &&
           x.url == y.url &&
           x.compression == y.compression;
  }

  inline bool
  operator!= (const manifest_archive& x, const manifest_archive& y) noexcept
  {
    return !(x == y);
  }

  inline bool
  operator== (const manifest& x, const manifest& y) noexcept
  {
    return x.format == y.format &&
           x.kind == y.kind &&
           x.archives == y.archives &&
           x.files == y.files;
  }

  inline bool
  operator!= (const manifest& x, const manifest& y) noexcept
  {
    return !(x == y);
  }

  // Stream output.
  //
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
