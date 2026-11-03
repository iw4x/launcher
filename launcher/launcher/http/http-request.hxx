// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include <launcher/http/http-types.hxx>

namespace launcher
{
  // HTTP request. The URL can be absolute or a path.
  //
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

    // Return the path and the query of the URL or / if there is no path.
    //
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

    // Add the Content-Length, Host, and User-Agent headers, unless present.
    //
    void
    normalize ();

    // Return true if there is a URL.
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

  // Print the request line, for example, GET https://iw4x.io/ HTTP/1.1.
  //
  std::ostream&
  operator << (std::ostream& o, const http_request& r);
}
