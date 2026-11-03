// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/download/download-request.hxx>

using namespace std;

namespace launcher
{
  download_request::
  download_request (string u, fs::path t)
    : urls ({move (u)}),
      target (move (t))
  {
  }

  download_request::
  download_request (vector<string> us, fs::path t)
    : urls (move (us)),
      target (move (t))
  {
  }

  bool download_request::
  valid () const
  {
    bool r (!urls.empty () && !target.empty ());
    return r;
  }

  bool
  operator== (const download_request& x, const download_request& y)
  {
    // Note that the order of the URLs matters since it is the order in which
    // we try them.
    //
    bool r (x.urls == y.urls && x.target == y.target);
    return r;
  }
}
