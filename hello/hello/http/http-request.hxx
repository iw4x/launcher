#pragma once

#include <string>
#include <utility>
#include <ostream>
#include <optional>

#include <hello/http/http-types.hxx>

namespace hello
{
  // HTTP request.
  //
  template <typename S, typename B = S>
  class basic_http_request
  {
  public:
    using string_type  = S;
    using body_type    = B;
    using headers_type = basic_http_headers<string_type>;

    http_method               method;
    string_type               url;
    http_version              version;
    headers_type              headers;
    std::optional<body_type>  body;

    // Constructors.
    //
    basic_http_request () = default;

    basic_http_request (http_method m,
                       string_type u,
                       http_version v = http_version (1, 1))
        : method (m), url (std::move (u)), version (v) {}

    basic_http_request (http_method m,
                       string_type u,
                       headers_type h,
                       http_version v = http_version (1, 1))
        : method (m),
          url (std::move (u)),
          version (v),
          headers (std::move (h)) {}

    basic_http_request (http_method m,
                       string_type u,
                       headers_type h,
                       body_type b,
                       http_version v = http_version (1, 1))
        : method (m),
          url (std::move (u)),
          version (v),
          headers (std::move (h)),
          body (std::move (b)) {}

    // Get the request target (path component of URL).
    //
    string_type
    target () const;

    // Set a header field.
    //
    void
    set_header (string_type name, string_type value)
    {
      headers.set (std::move (name), std::move (value));
    }

    // Get a header field.
    //
    std::optional<string_type>
    get_header (const string_type& name) const
    {
      return headers.get (name);
    }

    // Check if a header exists.
    //
    bool
    has_header (const string_type& name) const
    {
      return headers.contains (name);
    }

    // Set the content type.
    //
    void
    set_content_type (string_type ct)
    {
      set_header (string_type ("Content-Type"), std::move (ct));
    }

    // Set the user agent.
    //
    void
    set_user_agent (string_type ua)
    {
      set_header (string_type ("User-Agent"), std::move (ua));
    }

    // Set authorization header.
    //
    void
    set_authorization (string_type auth)
    {
      set_header (string_type ("Authorization"), std::move (auth));
    }

    // Set bearer token authorization.
    //
    void
    set_bearer_token (const string_type& token)
    {
      set_authorization (string_type ("Bearer ") + token);
    }

    // Set the body.
    //
    void
    set_body (body_type b)
    {
      body = std::move (b);
    }

    // Check if request has a body.
    //
    bool
    has_body () const noexcept
    {
      return body.has_value ();
    }

    // Normalize the request (e.g., add default headers).
    //
    void
    normalize ();

    // Check if the request is valid.
    //
    bool
    valid () const noexcept
    {
      return !url.empty ();
    }

    bool
    empty () const noexcept
    {
      return url.empty ();
    }
  };

  template <typename S, typename B>
  inline bool
  operator== (const basic_http_request<S, B>& x,
              const basic_http_request<S, B>& y) noexcept
  {
    return x.method == y.method &&
           x.url == y.url &&
           x.version == y.version &&
           x.headers == y.headers &&
           x.body == y.body;
  }

  template <typename S, typename B>
  inline bool
  operator!= (const basic_http_request<S, B>& x,
              const basic_http_request<S, B>& y) noexcept
  {
    return !(x == y);
  }

  template <typename S, typename B>
  inline auto
  operator<< (std::basic_ostream<typename S::value_type>& o,
              const basic_http_request<S, B>& r) -> decltype (o)
  {
    o << to_string (r.method) << ' '
      << r.url.string () << ' '
      << r.version;
    return o;
  }

  // Common typedefs.
  //
  using http_request = basic_http_request<std::string, std::string>;
}

#include <hello/http/http-request.ixx>
