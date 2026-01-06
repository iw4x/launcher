#pragma once

#include <string>
#include <utility>
#include <optional>

#include <hello/http/http-types.hxx>
#include <hello/http/http-request.hxx>
#include <hello/http/http-response.hxx>

namespace hello
{
  // HTTP API endpoint descriptor.
  //
  template <typename S = std::string>
  class basic_http_endpoint
  {
  public:
    using string_type = S;

    string_type  base_url;
    string_type  path_pattern;  // Can contain {param} placeholders.
    http_method  default_method;

    // Constructors.
    //
    basic_http_endpoint () : default_method (http_method::get) {}

    basic_http_endpoint (string_type base,
                        string_type pattern,
                        http_method method = http_method::get)
      : base_url (std::move (base)),
        path_pattern (std::move (pattern)),
        default_method (method) {}

    // Build a complete URL by replacing path parameters.
    //
    // Example: pattern "/users/{id}/posts" with params {"id", "123"}
    //          becomes "base_url/users/123/posts"
    //
    template <typename M>
    string_type
    build_url (const M& params) const;

    // Build URL without parameters.
    //
    string_type
    build_url () const;

    bool
    empty () const noexcept
    {
      return base_url.empty () && path_pattern.empty ();
    }
  };

  template <typename S>
  inline bool
  operator== (const basic_http_endpoint<S>& x,
              const basic_http_endpoint<S>& y) noexcept
  {
    return x.base_url == y.base_url &&
           x.path_pattern == y.path_pattern &&
           x.default_method == y.default_method;
  }

  template <typename S>
  inline bool
  operator!= (const basic_http_endpoint<S>& x,
              const basic_http_endpoint<S>& y) noexcept
  {
    return !(x == y);
  }

  // Common typedefs.
  //
  using http_endpoint = basic_http_endpoint<std::string>;
}

#include <hello/http/http-endpoint.ixx>
