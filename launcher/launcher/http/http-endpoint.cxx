// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/http/http-endpoint.hxx>
#include <stdexcept>

using namespace std;

namespace launcher
{
  http_endpoint::
  http_endpoint ()
    : default_method (http_method::get)
  {
  }

  http_endpoint::
  http_endpoint (string b, string p, http_method m)
    : base_url (move (b)),
      path_pattern (move (p)),
      default_method (m)
  {
  }

  string http_endpoint::
  build_url (const map<string, string>& ps) const
  {
    string p (path_pattern);

    // Note that a placeholder can appear more than once. We continue after
    // the value so that a value containing the placeholder doesn't loop.
    //
    for (const auto& [k, v] : ps)
    {
      string t ("{" + k + "}");
      string::size_type i (0);

      while ((i = p.find (t, i)) != string::npos)
      {
        p.replace (i, t.size (), v);
        i += v.size ();
      }
    }

    if (p.find ('{') != string::npos)
      throw invalid_argument ("missing parameters for endpoint path");

    string r (base_url);

    // Join with exactly one /.
    //
    if (!p.empty ())
    {
      if (!r.empty () && r.back () != '/')
        r += '/';

      if (p.front () == '/')
        p.erase (0, 1);

      r += p;
    }

    return r;
  }

  string http_endpoint::
  build_url () const
  {
    if (path_pattern.find ('{') != string::npos)
      throw invalid_argument ("endpoint requires parameters but none provided");

    string r (base_url);

    // As above.
    //
    if (!path_pattern.empty ())
    {
      if (!r.empty () && r.back () != '/')
        r += '/';

      string p (path_pattern);

      if (p.front () == '/')
        p.erase (0, 1);

      r += p;
    }

    return r;
  }

  bool
  operator== (const http_endpoint& x, const http_endpoint& y) noexcept
  {
    return x.base_url == y.base_url &&
           x.path_pattern == y.path_pattern &&
           x.default_method == y.default_method;
  }
}
