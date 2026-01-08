#pragma once

#include <string>
#include <utility>
#include <ostream>
#include <optional>

#include <launcher/http/http-types.hxx>

namespace launcher
{
  // HTTP response.
  //
  template <typename S, typename B = S>
  class basic_http_response
  {
  public:
    using string_type  = S;
    using body_type    = B;
    using headers_type = basic_http_headers<string_type>;

    http_status           status;
    http_version          version;
    string_type           reason;  // Status reason phrase.
    headers_type          headers;
    std::optional<body_type> body;

    // Constructors.
    //
    basic_http_response () : status (http_status::ok) {}

    basic_http_response (http_status s,
                         http_version v = http_version (1, 1))
      : status (s), version (v) {}

    basic_http_response (http_status s,
                         string_type r,
                         http_version v = http_version (1, 1))
      : status (s), version (v), reason (std::move (r)) {}

    basic_http_response (http_status s,
                         headers_type h,
                         http_version v = http_version (1, 1))
      : status (s), version (v), headers (std::move (h)) {}

    basic_http_response (http_status s,
                         headers_type h,
                         body_type b,
                         http_version v = http_version (1, 1))
      : status (s),
        version (v),
        headers (std::move (h)),
        body (std::move (b)) {}

    // Status code checking helpers.
    //
    bool
    is_informational () const noexcept
    {
      return status_code () >= 100 && status_code () < 200;
    }

    bool
    is_success () const noexcept
    {
      return status_code () >= 200 && status_code () < 300;
    }

    bool
    is_redirection () const noexcept
    {
      return status_code () >= 300 && status_code () < 400;
    }

    bool
    is_client_error () const noexcept
    {
      return status_code () >= 400 && status_code () < 500;
    }

    bool
    is_server_error () const noexcept
    {
      return status_code () >= 500 && status_code () < 600;
    }

    bool
    is_error () const noexcept
    {
      return status_code () >= 400;
    }

    std::uint16_t
    status_code () const noexcept
    {
      return static_cast<std::uint16_t> (status);
    }

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

    // Get content type.
    //
    std::optional<string_type>
    content_type () const
    {
      return get_header (string_type ("Content-Type"));
    }

    // Get content length.
    //
    std::optional<std::uint64_t>
    content_length () const;

    // Get location (for redirects).
    //
    std::optional<string_type>
    location () const
    {
      return get_header (string_type ("Location"));
    }

    // Set the body.
    //
    void
    set_body (body_type b)
    {
      body = std::move (b);
    }

    // Check if response has a body.
    //
    bool
    has_body () const noexcept
    {
      return body.has_value ();
    }

    // Check if the response is valid.
    //
    bool
    valid () const noexcept
    {
      return status_code () > 0;
    }

    bool
    empty () const noexcept
    {
      return status_code () == 0;
    }
  };

  template <typename S, typename B>
  inline bool
  operator== (const basic_http_response<S, B>& x,
              const basic_http_response<S, B>& y) noexcept
  {
    return x.status == y.status &&
           x.version == y.version &&
           x.reason == y.reason &&
           x.headers == y.headers &&
           x.body == y.body;
  }

  template <typename S, typename B>
  inline bool
  operator!= (const basic_http_response<S, B>& x,
              const basic_http_response<S, B>& y) noexcept
  {
    return !(x == y);
  }

  template <typename S, typename B>
  inline auto
  operator<< (std::basic_ostream<typename S::value_type>& o,
              const basic_http_response<S, B>& r) -> decltype (o)
  {
    o << r.version << ' '
      << static_cast<std::uint16_t> (r.status);

    if (!r.reason.empty ())
      o << ' ' << r.reason;

    return o;
  }

  // Common typedefs.
  //
  using http_response = basic_http_response<std::string, std::string>;
}

#include <launcher/http/http-response.ixx>
