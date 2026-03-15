#pragma once

#include <string>
#include <optional>
#include <map>
#include <cstdint>

namespace launcher
{
  // GitHub API HTTP request parameters.
  //
  struct github_request
  {
    // HTTP method.
    //
    enum class method_type
    {
      get,
      post,
      put,
      patch,
      delete_
    };

    method_type method;
    std::string endpoint;
    std::optional<std::string> token; // Bearer token
    std::optional<std::string> body;  // Request body (JSON)
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;

    github_request ();
    github_request (method_type m, std::string ep);

    // Builder methods.
    //
    github_request&
    with_token (std::string t);

    github_request&
    with_body (std::string b);

    github_request&
    with_header (std::string key, std::string value);

    github_request&
    with_query (std::string key, std::string value);

    // Common query parameters.
    //
    github_request&
    with_per_page (std::uint32_t n);

    github_request&
    with_page (std::uint32_t n);

    github_request&
    with_state (const std::string& state); // "open", "closed", "all"

    github_request&
    with_sort (const std::string& sort); // "created", "updated", "pushed"

    github_request&
    with_direction (const std::string& dir); // "asc", "desc"

    // Build full URL with query parameters.
    //
    std::string
    url () const;

    // Get HTTP method as string.
    //
    std::string
    method_string () const;
  };
}
