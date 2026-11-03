// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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

  // Coroutine wrappers of manifest::parse_async() and
  // manifest::validate_async().
  //
  // @@ The *_parallel() functions process the manifests one after another.
  //    Nothing outside manifest-parser.cxx uses any of these functions.
  //
  class manifest_parser
  {
  public:
    static asio::awaitable<manifest>
    parse (const std::string& json_str,
           manifest_format kind = manifest_format::update);

    static asio::awaitable<std::vector<manifest>>
    parse_parallel (const std::vector<std::string>& json_strings,
                    manifest_format kind = manifest_format::update);

    static asio::awaitable<bool>
    validate (const manifest& m);

    static asio::awaitable<std::vector<bool>>
    validate_parallel (const std::vector<manifest>& manifests);
  };

  asio::awaitable<manifest>
  parse_update_manifest (const std::string& json_str);

  asio::awaitable<manifest>
  parse_dlc_manifest (const std::string& json_str);

  asio::awaitable<bool>
  validate_manifest (const manifest& m);
}
