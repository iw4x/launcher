#pragma once

#include <string>
#include <optional>
#include <map>
#include <cstdint>

namespace hello
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

    github_request ()
      : method (method_type::get) {}

    github_request (method_type m, std::string ep)
      : method (m), endpoint (std::move (ep)) {}

    // Builder methods.
    //
    github_request&
    with_token (std::string t)
    {
      token = std::move (t);
      return *this;
    }

    github_request&
    with_body (std::string b)
    {
      body = std::move (b);
      return *this;
    }

    github_request&
    with_header (std::string key, std::string value)
    {
      headers[std::move (key)] = std::move (value);
      return *this;
    }

    github_request&
    with_query (std::string key, std::string value)
    {
      query_params[std::move (key)] = std::move (value);
      return *this;
    }

    // Common query parameters.
    //
    github_request&
    with_per_page (std::uint32_t n)
    {
      return with_query ("per_page", std::to_string (n));
    }

    github_request&
    with_page (std::uint32_t n)
    {
      return with_query ("page", std::to_string (n));
    }

    github_request&
    with_state (const std::string& state) // "open", "closed", "all"
    {
      return with_query ("state", state);
    }

    github_request&
    with_sort (const std::string& sort) // "created", "updated", "pushed"
    {
      return with_query ("sort", sort);
    }

    github_request&
    with_direction (const std::string& dir) // "asc", "desc"
    {
      return with_query ("direction", dir);
    }

    // Build full URL with query parameters.
    //
    std::string
    url () const;

    // Get HTTP method as string.
    //
    std::string
    method_string () const
    {
      switch (method)
      {
        case method_type::get:     return "GET";
        case method_type::post:    return "POST";
        case method_type::put:     return "PUT";
        case method_type::patch:   return "PATCH";
        case method_type::delete_: return "DELETE";
      }
      return "GET";
    }
  };
}
