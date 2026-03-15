#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include <launcher/http/http-types.hxx>

namespace launcher
{
  class http_response
  {
  public:
    http_status status;
    http_version version;
    std::string reason;
    http_headers headers;
    std::optional<std::string> body;

    http_response ();

    explicit
    http_response (http_status s, http_version v = http_version (1, 1));

    explicit
    http_response (http_status s,
                   std::string r,
                   http_version v = http_version (1, 1));

    explicit
    http_response (http_status s,
                   http_headers h,
                   http_version v = http_version (1, 1));

    explicit
    http_response (http_status s,
                   http_headers h,
                   std::string b,
                   http_version v = http_version (1, 1));

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

    void
    set_header (std::string name, std::string value)
    {
      headers.set (std::move (name), std::move (value));
    }

    std::optional<std::string>
    get_header (const std::string& name) const
    {
      return headers.get (name);
    }

    bool
    has_header (const std::string& name) const
    {
      return headers.contains (name);
    }

    std::optional<std::string>
    content_type () const
    {
      return get_header ("Content-Type");
    }

    std::optional<std::uint64_t>
    content_length () const;

    std::optional<std::string>
    location () const
    {
      return get_header ("Location");
    }

    void
    set_body (std::string b)
    {
      body = std::move (b);
    }

    bool
    has_body () const noexcept
    {
      return body.has_value ();
    }

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

  bool
  operator == (const http_response& x, const http_response& y) noexcept;

  bool
  operator != (const http_response& x, const http_response& y) noexcept;

  std::ostream&
  operator << (std::ostream& o, const http_response& r);
}
