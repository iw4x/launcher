#pragma once

#include <launcher/manifest/manifest.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <string>
#include <vector>

namespace launcher
{
  namespace asio = boost::asio;

  // Async manifest parser with parallel processing.
  //
  class manifest_parser
  {
  public:
    // Parse single manifest asynchronously.
    //
    static asio::awaitable<manifest>
    parse (const std::string& json_str,
           manifest_format kind = manifest_format::update);

    // Parse multiple manifests in parallel.
    //
    static asio::awaitable<std::vector<manifest>>
    parse_parallel (const std::vector<std::string>& json_strings,
                    manifest_format kind = manifest_format::update);

    // Validate manifest asynchronously.
    //
    static asio::awaitable<bool>
    validate (const manifest& m);

    // Validate multiple manifests in parallel.
    //
    static asio::awaitable<std::vector<bool>>
    validate_parallel (const std::vector<manifest>& manifests);
  };

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
