// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/utility.hxx>

#ifdef _WIN32
#  include <windows.h>
#endif

using namespace std;
using namespace std::filesystem;

namespace launcher
{
  string
  to_utf8 (const path& p)
  {
    u8string s (p.u8string ());
    return string (s.begin (), s.end ());
  }

  path
  from_utf8 (string_view s)
  {
    return path (reinterpret_cast<const char8_t*> (s.data ()),
                 reinterpret_cast<const char8_t*> (s.data () + s.size ()));
  }

  string
  to_utf8_system_message ([[maybe_unused]] const string& s)
  {
#ifdef _WIN32
    // Convert from the ANSI code page to UTF-16 and then to UTF-8. If either
    // conversion fails, then the best we can do is return the message as is.
    //
    int n (static_cast<int> (s.size ()));
    int wn (MultiByteToWideChar (CP_ACP, 0, s.data (), n, nullptr, 0));

    if (wn > 0)
    {
      wstring w (static_cast<size_t> (wn), L'\0');
      MultiByteToWideChar (CP_ACP, 0, s.data (), n, w.data (), wn);

      int un (WideCharToMultiByte (
        CP_UTF8, 0, w.data (), wn, nullptr, 0, nullptr, nullptr));

      if (un > 0)
      {
        string r (static_cast<size_t> (un), '\0');
        WideCharToMultiByte (
          CP_UTF8, 0, w.data (), wn, r.data (), un, nullptr, nullptr);
        return r;
      }
    }
#endif

    return s;
  }
}
