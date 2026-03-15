#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include <launcher/http/http-types.hxx>

namespace launcher
{
  class http_request
  {
  public:
    http_method method;
    std::string url;
    http_version version;
    http_headers headers;
    std::optional<std::string> body;

    http_request () = default;

    explicit
    http_request (http_method m,
                  std::string u,
                  http_version v = http_version (1, 1));

    explicit
    http_request (http_method m,
                  std::string u,
                  http_headers h,
                  http_version v = http_version (1, 1));

    explicit
    http_request (http_method m,
                  std::string u,
                  http_headers h,
                  std::string b,
                  http_version v = http_version (1, 1));

    std::string
    target () const;

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

    void
    set_content_type (std::string ct)
    {
      set_header ("Content-Type", std::move (ct));
    }

    void
    set_user_agent (std::string ua)
    {
      set_header ("User-Agent", std::move (ua));
    }

    void
    set_authorization (std::string auth)
    {
      set_header ("Authorization", std::move (auth));
    }

    void
    set_bearer_token (const std::string& token)
    {
      set_authorization ("Bearer " + token);
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

    void
    normalize ();

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

  bool
  operator == (const http_request& x, const http_request& y) noexcept;

  bool
  operator != (const http_request& x, const http_request& y) noexcept;

  std::ostream&
  operator << (std::ostream& o, const http_request& r);
}
