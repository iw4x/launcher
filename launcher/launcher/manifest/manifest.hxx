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
  template <typename S = std::string>
  struct basic_hash
  {
    using string_type = S;

    hash_algorithm algorithm;
    string_type value;

    basic_hash () : algorithm (hash_algorithm::blake3) {}

    basic_hash (hash_algorithm a, string_type v)
      : algorithm (a), value (std::move (v)) {}

    explicit basic_hash (string_type v)
      : algorithm (hash_algorithm::blake3), value (std::move (v)) {}

    bool
    empty () const noexcept
    {
      return value.empty ();
    }

    // String representation.
    //
    string_type
    string () const;

    // Verify hash against data.
    //
    template <typename Buffer>
    bool
    verify (const Buffer&) const;
  };

  // File entry in a manifest.
  //
  template <typename S = std::string, typename H = basic_hash<S>>
  struct basic_manifest_file
  {
    using string_type = S;
    using hash_type = H;
    using size_type = std::uint64_t;

    hash_type hash;
    size_type size;
    string_type path;
    std::optional<string_type> asset_name;
    std::optional<string_type> archive_name;

    basic_manifest_file () : size (0) {}

    basic_manifest_file (hash_type h,
                         size_type s,
                         string_type p,
                         std::optional<string_type> a = std::nullopt,
                         std::optional<string_type> ar = std::nullopt)
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
  template <typename S = std::string, typename H = basic_hash<S>>
  struct basic_manifest_archive
  {
    using string_type = S;
    using hash_type = H;
    using size_type = std::uint64_t;
    using file_type = basic_manifest_file<S, H>;

    hash_type hash;
    size_type size;
    string_type name;
    string_type url;
    compression_type compression;
    std::vector<file_type> files;

    basic_manifest_archive ()
      : size (0), compression (compression_type::none) {}

    basic_manifest_archive (hash_type h,
                            size_type s,
                            string_type n,
                            string_type u = string_type (),
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

  // Manifest traits for customization.
  //
  template <typename F, // Format type
            typename S = std::string,
            typename H = basic_hash<S>>
  struct manifest_traits
  {
    using format_type = F;
    using string_type = S;
    using hash_type = H;
    using file_type = basic_manifest_file<S, H>;
    using archive_type = basic_manifest_archive<S, H>;

    // Parse format from string representation.
    //
    static std::optional<format_type>
    translate_format (const string_type& /* url     */,
                      const string_type& /* content */,
                      manifest_format&   /* kind    */)
    {
      return std::nullopt;
    }

    // Translate format to string representation.
    //
    static string_type
    translate_format (const format_type&,
                      manifest_format /* kind */)
    {
      return string_type ();
    }

    // Validate file entry.
    //
    static bool
    validate_file (const file_type&)
    {
      return true;
    }

    // Validate archive entry.
    //
    static bool
    validate_archive (const archive_type&)
    {
      return true;
    }
  };

  // Main manifest class.
  //
  template <typename F, // Format type
            typename T = manifest_traits<F>>
  class basic_manifest
  {
  public:
    using traits_type = T;
    using format_type = typename traits_type::format_type;
    using string_type = typename traits_type::string_type;
    using hash_type = typename traits_type::hash_type;
    using file_type = typename traits_type::file_type;
    using archive_type = typename traits_type::archive_type;

    format_type format;
    manifest_format kind;
    std::vector<archive_type> archives;
    std::vector<file_type> files;

    // Constructors.
    //
    basic_manifest () : kind (manifest_format::update) {}

    explicit
    basic_manifest (format_type f, manifest_format k = manifest_format::update)
      : format (std::move (f)), kind (k) {}

    // Parse from JSON string.
    //
    explicit
    basic_manifest (const string_type& json_str,
                    manifest_format k = manifest_format::update);

    // Parse from JSON value.
    //
    explicit
    basic_manifest (const json::value& jv,
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
    string_type
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
    static asio::awaitable<basic_manifest>
    parse_async (const string_type& json_str,
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

  // Type aliases for common instantiations.
  //
  using hash = basic_hash<std::string>;
  using manifest_file = basic_manifest_file<std::string>;
  using manifest_archive = basic_manifest_archive<std::string>;
  using manifest = basic_manifest<manifest_format>;

  // Comparison operators.
  //
  template <typename S>
  inline bool
  operator== (const basic_hash<S>& x, const basic_hash<S>& y) noexcept
  {
    return x.algorithm == y.algorithm && x.value == y.value;
  }

  template <typename S>
  inline bool
  operator!= (const basic_hash<S>& x, const basic_hash<S>& y) noexcept
  {
    return !(x == y);
  }

  template <typename S, typename H>
  inline bool
  operator== (const basic_manifest_file<S, H>& x,
              const basic_manifest_file<S, H>& y) noexcept
  {
    return x.hash == y.hash &&
           x.size == y.size &&
           x.path == y.path &&
           x.asset_name == y.asset_name &&
           x.archive_name == y.archive_name;
  }

  template <typename S, typename H>
  inline bool
  operator!= (const basic_manifest_file<S, H>& x,
              const basic_manifest_file<S, H>& y) noexcept
  {
    return !(x == y);
  }

  template <typename S, typename H>
  inline bool
  operator== (const basic_manifest_archive<S, H>& x,
              const basic_manifest_archive<S, H>& y) noexcept
  {
    return x.hash == y.hash &&
           x.size == y.size &&
           x.name == y.name &&
           x.url == y.url &&
           x.compression == y.compression;
  }

  template <typename S, typename H>
  inline bool
  operator!= (const basic_manifest_archive<S, H>& x,
              const basic_manifest_archive<S, H>& y) noexcept
  {
    return !(x == y);
  }

  template <typename F, typename T>
  inline bool
  operator== (const basic_manifest<F, T>& x,
              const basic_manifest<F, T>& y) noexcept
  {
    return x.format == y.format &&
           x.kind == y.kind &&
           x.archives == y.archives &&
           x.files == y.files;
  }

  template <typename F, typename T>
  inline bool
  operator!= (const basic_manifest<F, T>& x,
              const basic_manifest<F, T>& y) noexcept
  {
    return !(x == y);
  }

  // Stream output.
  //
  template <typename S>
  inline auto
  operator<< (std::basic_ostream<typename S::value_type>& o,
              const basic_hash<S>& h) -> decltype (o)
  {
    return o << h.string ();
  }

  template <typename F, typename T>
  inline auto
  operator<< (std::basic_ostream<typename T::string_type::value_type>& o,
              const basic_manifest<F, T>& m) -> decltype (o)
  {
    return o << m.string ();
  }
}

#include <launcher/manifest/manifest.ixx>
#include <launcher/manifest/manifest.txx>
