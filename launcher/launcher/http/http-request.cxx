// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/http/http-request.hxx>

using namespace std;

namespace launcher
{
  http_request::http_request (http_method m, string u, http_version v)
    : method (m),
      url (move (u)),
      version (v)
  {
  }

  http_request::http_request (http_method m,
                              string u,
                              http_headers h,
                              http_version v)
    : method (m),
      url (move (u)),
      version (v),
      headers (move (h))
  {
  }

  http_request::http_request (http_method m,
                              string u,
                              http_headers h,
                              string b,
                              http_version v)
    : method (m),
      url (move (u)),
      version (v),
      headers (move (h)),
      body (move (b))
  {
  }

  string
  http_request::target () const
  {
    // Skip the scheme so that we find the / after the authority.
    //
    size_t e (url.find ("://"));
    size_t p (e != string::npos ? e + 3 : 0);

    size_t s (url.find ('/', p));

    return s != string::npos ? url.substr (s) : "/";
  }

  void
  http_request::normalize ()
  {
    if (body && !has_header ("Content-Length") &&
        !has_header ("Transfer-Encoding"))
      set_header ("Content-Length", std::to_string (body->size ()));

    // Note that the Host header omits the port.
    //
    // @@ The server expects the port if it is not the default one.
    //
    if (!has_header ("Host"))
    {
      size_t e (url.find ("://"));
      size_t p (e != string::npos ? e + 3 : 0);

      size_t n (url.find_first_of ("/:", p));
      size_t l (n == string::npos ? url.size () - p : n - p);

      string h (url.substr (p, l));

      if (!h.empty ())
        set_header ("Host", h);
    }

    if (!has_header ("User-Agent"))
      set_header ("User-Agent", "IW4x-Launcher/1.1");
  }

  ostream&
  operator << (ostream& o, const http_request& r)
  {
    o << to_string (r.method) << ' ' << r.url << ' ' << r.version;
    return o;
  }
}
