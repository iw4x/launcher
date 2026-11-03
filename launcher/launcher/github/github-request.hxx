// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <optional>
#include <map>
#include <cstdint>

namespace launcher
{
  // GitHub API request, built up with the with_*() functions.
  //
  struct github_request
  {
    enum class method_type
    {
      get,
      post,
      put,
      patch,
      delete_
    };

    method_type method;
    std::string endpoint;             // See github_endpoint.
    std::optional<std::string> token; // Overrides the github_api token.
    std::optional<std::string> body;  // JSON.
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;

    github_request ();
    github_request (method_type m, std::string ep);

    github_request&
    with_token (std::string t);

    github_request&
    with_body (std::string b);

    github_request&
    with_header (std::string key, std::string value);

    // Note that the key and the value are used as is, without encoding.
    //
    github_request&
    with_query (std::string key, std::string value);

    github_request&
    with_per_page (std::uint32_t n);

    github_request&
    with_page (std::uint32_t n);

    github_request&
    with_state (const std::string& state); // open, closed, or all.

    github_request&
    with_sort (const std::string& sort); // created, updated, or pushed.

    github_request&
    with_direction (const std::string& dir); // asc or desc.

    // Return the endpoint with the query parameters appended.
    //
    std::string
    url () const;

    // Return the method name in upper case, for example, GET.
    //
    std::string
    method_string () const;
  };
}
