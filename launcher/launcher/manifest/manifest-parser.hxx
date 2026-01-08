#pragma once

#include <launcher/manifest/manifest.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <string>
#include <vector>
#include <memory>

namespace launcher
{
  namespace asio = boost::asio;

  // Async manifest parser with parallel processing.
  //
  template <typename F = manifest_format, typename T = manifest_traits<F>>
  class basic_manifest_parser
  {
  public:
    using manifest_type = basic_manifest<F, T>;
    using string_type = typename T::string_type;

    // Parse single manifest asynchronously.
    //
    static asio::awaitable<manifest_type>
    parse (const string_type& json_str,
           manifest_format kind = manifest_format::update);

    // Parse multiple manifests in parallel.
    //
    static asio::awaitable<std::vector<manifest_type>>
    parse_parallel (const std::vector<string_type>& json_strings,
                   manifest_format kind = manifest_format::update);

    // Validate manifest asynchronously.
    //
    static asio::awaitable<bool>
    validate (const manifest_type& manifest);

    // Validate multiple manifests in parallel.
    //
    static asio::awaitable<std::vector<bool>>
    validate_parallel (const std::vector<manifest_type>& manifests);
  };

  // Type alias for common instantiation.
  //
  using manifest_parser = basic_manifest_parser<manifest_format>;

  // Parse update manifest from JSON string.
  //
  asio::awaitable<manifest>
  parse_update_manifest (const std::string& json_str);

  // Parse DLC manifest from JSON string.
  //
  asio::awaitable<manifest>
  parse_dlc_manifest (const std::string& json_str);

  // Validate manifest with full integrity checks.
  //
  asio::awaitable<bool>
  validate_manifest (const manifest& m);
}

#include <launcher/manifest/manifest-parser.txx>
