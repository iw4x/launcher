// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/manifest/manifest-parser.hxx>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace launcher
{
  using namespace asio::experimental::awaitable_operators;

  asio::awaitable<manifest> manifest_parser::
  parse (std::string json_str, manifest_format kind)
  {
    co_return co_await manifest::parse_async (json_str, kind);
  }

  asio::awaitable<std::vector<manifest>> manifest_parser::
  parse_parallel (std::vector<std::string> json_strings,
                  manifest_format kind)
  {
    std::vector<manifest> r;
    r.reserve (json_strings.size ());

    for (const auto& json_str : json_strings)
    {
      auto m (co_await parse (json_str, kind));
      r.push_back (std::move (m));
    }

    co_return r;
  }

  asio::awaitable<bool> manifest_parser::
  validate (manifest m)
  {
    co_return co_await m.validate_async ();
  }

  asio::awaitable<std::vector<bool>> manifest_parser::
  validate_parallel (std::vector<manifest> manifests)
  {
    std::vector<bool> r;
    r.reserve (manifests.size ());

    for (const auto& m : manifests)
    {
      bool v (co_await validate (m));
      r.push_back (v);
    }

    co_return r;
  }

  asio::awaitable<manifest>
  parse_update_manifest (std::string json_str)
  {
    co_return co_await manifest_parser::parse (json_str, manifest_format::update);
  }

  asio::awaitable<manifest>
  parse_dlc_manifest (std::string json_str)
  {
    co_return co_await manifest_parser::parse (json_str, manifest_format::dlc);
  }

  asio::awaitable<bool>
  validate_manifest (manifest m)
  {
    co_return co_await manifest_parser::validate (m);
  }
}
