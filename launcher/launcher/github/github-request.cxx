// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/github/github-request.hxx>

#include <sstream>

using namespace std;

namespace launcher
{
  github_request::
  github_request ()
    : method (method_type::get)
  {
  }

  github_request::
  github_request (method_type m, string e)
    : method (m), endpoint (move (e))
  {
  }

  github_request& github_request::
  with_token (string t)
  {
    token = move (t);
    return *this;
  }

  github_request& github_request::
  with_body (string b)
  {
    body = move (b);
    return *this;
  }

  github_request& github_request::
  with_header (string k, string v)
  {
    headers[move (k)] = move (v);
    return *this;
  }

  github_request& github_request::
  with_query (string k, string v)
  {
    query_params[move (k)] = move (v);
    return *this;
  }

  github_request& github_request::
  with_per_page (uint32_t n)
  {
    return with_query ("per_page", to_string (n));
  }

  github_request& github_request::
  with_page (uint32_t n)
  {
    return with_query ("page", to_string (n));
  }

  github_request& github_request::
  with_state (const string& s)
  {
    return with_query ("state", s);
  }

  github_request& github_request::
  with_sort (const string& s)
  {
    return with_query ("sort", s);
  }

  github_request& github_request::
  with_direction (const string& d)
  {
    return with_query ("direction", d);
  }

  string github_request::
  url () const
  {
    ostringstream os;
    os << endpoint;

    if (!query_params.empty ())
    {
      bool f (true); // First parameter.

      for (const auto& [k, v] : query_params)
      {
        os << (f ? '?' : '&');
        os << k << '=' << v;
        f = false;
      }
    }

    return os.str ();
  }

  string github_request::
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

    return "GET"; // Unreachable.
  }
}
