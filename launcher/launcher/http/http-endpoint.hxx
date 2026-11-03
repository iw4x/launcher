// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <string>
#include <utility>

#include <launcher/http/http-request.hxx>
#include <launcher/http/http-response.hxx>
#include <launcher/http/http-types.hxx>

namespace launcher
{
  // Endpoint URL made of a base URL and a path pattern with {<name>}
  // placeholders, for example, /repos/{owner}/{repo}.
  //
  class http_endpoint
  {
  public:
    std::string base_url;
    std::string path_pattern;
    http_method default_method;

    http_endpoint ();

    explicit
    http_endpoint (std::string base,
                   std::string pattern,
                   http_method method = http_method::get);

    // Return the base URL and the path, joined with a single /, with the
    // placeholders replaced with the parameter values. Throw
    // std::invalid_argument if a placeholder has no value.
    //
    std::string
    build_url (const std::map<std::string, std::string>& params) const;

    // As above but for a pattern without placeholders.
    //
    std::string
    build_url () const;

    bool
    empty () const noexcept
    {
      return base_url.empty () && path_pattern.empty ();
    }
  };

  bool
  operator == (const http_endpoint& x, const http_endpoint& y) noexcept;
}
