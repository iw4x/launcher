#pragma once

#include <map>
#include <string>
#include <utility>

#include <launcher/http/http-request.hxx>
#include <launcher/http/http-response.hxx>
#include <launcher/http/http-types.hxx>

namespace launcher
{
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

    std::string
    build_url (const std::map<std::string, std::string>& params) const;

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

  bool
  operator != (const http_endpoint& x, const http_endpoint& y) noexcept;
}
