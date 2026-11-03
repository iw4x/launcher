// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/update/update-types.hxx>

#include <cctype>
#include <sstream>

#include <launcher/launcher-log.hxx>

using namespace std;

namespace launcher
{
  // Parse the decimal number at position p, advancing past it. Return
  // nullopt if there is no digit at p.
  //
  // @@ We don't check for overflow.
  //
  static optional<uint64_t>
  parse_u64 (const string& s, size_t& p)
  {
    if (p >= s.size () || !isdigit (static_cast<unsigned char> (s[p])))
      return nullopt;

    uint64_t r (0);
    while (p < s.size () && isdigit (static_cast<unsigned char> (s[p])))
    {
      r = r * 10 + (s[p] - '0');
      ++p;
    }
    return r;
  }

  // Skip the c character at position p. Return false if it is not there.
  //
  static bool
  parse_c (const string& s, size_t& p, char c)
  {
    if (p < s.size () && s[p] == c)
    {
      ++p;
      return true;
    }
    return false;
  }

  int launcher_version::
  compare (const launcher_version& v) const noexcept
  {
    // Compare the major, minor, and patch numbers first.
    //
    if (major != v.major) return major < v.major ? -1 : 1;
    if (minor != v.minor) return minor < v.minor ? -1 : 1;
    if (patch != v.patch) return patch < v.patch ? -1 : 1;

    // Now it gets a bit tricky: 0 in pre_release means a final release,
    // which comes after every pre-release of the same version.
    //
    bool tr (pre_release == 0 && snapshot_sn == 0);
    bool vr (v.pre_release == 0 && v.snapshot_sn == 0);

    // If one is a release and the other isn't, the release wins.
    //
    if (tr != vr)
      return tr ? 1 : -1;

    // Both are pre-releases (or both are releases). Since betas are offset
    // by 500, comparing the numbers puts alphas before betas.
    //
    if (pre_release != v.pre_release)
      return pre_release < v.pre_release ? -1 : 1;

    // Snapshots are ordered after the corresponding pre-release.
    //
    if (snapshot_sn != v.snapshot_sn)
      return snapshot_sn < v.snapshot_sn ? -1 : 1;

    return 0;
  }

  string launcher_version::
  string () const
  {
    ostringstream o;
    o << major << '.' << minor << '.' << patch;

    // Undo the beta offset (see pre_release).
    //
    if (pre_release > 0)
    {
      if (is_beta ()) o << "-b." << (pre_release - 500);
      else            o << "-a." << pre_release;

      if (snapshot_sn != 0)
      {
        o << '.' << snapshot_sn;
        if (!snapshot_id.empty ())
          o << '.' << snapshot_id;
      }
    }

    return o.str ();
  }

  optional<launcher_version>
  parse_launcher_version (const std::string& s)
  {
    launcher::log::trace_l3 (categories::update{}, "attempting to parse version string: {}", s);

    if (s.empty ())
    {
      launcher::log::trace_l3 (categories::update{}, "version string is empty");
      return nullopt;
    }

    size_t p (0);

    // Be lenient with the 'v' prefix (e.g., git tags).
    //
    if (s[p] == 'v' || s[p] == 'V')
    {
      launcher::log::trace_l3 (categories::update{}, "skipped 'v' prefix in version string");
      ++p;
    }

    // We expect the standard triad (X.Y.Z).
    //
    auto mj (parse_u64 (s, p));
    if (!mj || !parse_c (s, p, '.'))
    {
      launcher::log::trace_l3 (categories::update{}, "failed to parse major version");
      return nullopt;
    }

    auto mi (parse_u64 (s, p));
    if (!mi || !parse_c (s, p, '.'))
    {
      launcher::log::trace_l3 (categories::update{}, "failed to parse minor version");
      return nullopt;
    }

    auto pa (parse_u64 (s, p));
    if (!pa)
    {
      launcher::log::trace_l3 (categories::update{}, "failed to parse patch version");
      return nullopt;
    }

    launcher_version v;
    v.major = static_cast<uint32_t> (*mj);
    v.minor = static_cast<uint32_t> (*mi);
    v.patch = static_cast<uint32_t> (*pa);

    // If we hit the end, it's a final release.
    //
    if (p >= s.size ())
    {
      launcher::log::trace_l3 (categories::update{}, "parsed release version: {}.{}.{}", v.major, v.minor, v.patch);
      return v;
    }

    if (!parse_c (s, p, '-'))
    {
      launcher::log::trace_l3 (categories::update{}, "unexpected character after patch version");
      return v; // Return what we have so far.
    }

    // Parse pre-release type. We only support alpha (a) and beta (b).
    //
    if (p >= s.size ()) return nullopt;

    char t (s[p]); // Type.
    if (t != 'a' && t != 'b')
    {
      launcher::log::trace_l3 (categories::update{}, "unsupported pre-release type: {}", t);
      return nullopt;
    }
    ++p;

    if (!parse_c (s, p, '.'))
    {
      launcher::log::trace_l3 (categories::update{}, "expected '.' after pre-release type");
      return nullopt;
    }

    // Offset betas by 500 (see pre_release).
    //
    auto pn (parse_u64 (s, p));
    if (!pn || *pn > 499)
    {
      launcher::log::trace_l3 (categories::update{}, "invalid or out-of-range pre-release number");
      return nullopt;
    }

    v.pre_release = static_cast<uint16_t> (t == 'b' ? *pn + 500 : *pn);

    // Check for snapshot suffix.
    //
    if (p >= s.size () || !parse_c (s, p, '.'))
    {
      launcher::log::trace_l3 (categories::update{}, "parsed pre-release version: {}", v.string ());
      return v;
    }

    // The .z placeholder stands for the snapshot of a development version
    // (e.g., 1.2.0-a.1.z). Give it the smallest sequence number, so that it
    // sorts as a snapshot.
    //
    if (p < s.size () && (s[p] == 'z' || s[p] == 'Z'))
    {
      ++p;
      v.snapshot_sn = 1; // Smallest snapshot.
      launcher::log::trace_l3 (categories::update{}, "parsed build2 placeholder '.z' snapshot version: {}", v.string ());
      return v;
    }

    // Parse snapshot sequence (YYYYMMDDhhmmss).
    //
    auto sn (parse_u64 (s, p));
    if (!sn)
    {
      launcher::log::trace_l3 (categories::update{}, "failed to parse snapshot sequence number");
      return nullopt;
    }

    v.snapshot_sn = *sn;

    // Whatever follows the next dot is the snapshot id. We stop at the
    // delimiters that could follow it, for good measure.
    //
    if (p < s.size () && parse_c (s, p, '.'))
    {
      size_t e (p);
      while (e < s.size () && s[e] != '-' && s[e] != '+' && s[e] != '/')
        ++e;

      v.snapshot_id = s.substr (p, e - p);
    }

    launcher::log::trace_l3 (categories::update{}, "parsed full snapshot version: {}", v.string ());
    return v;
  }

  platform_type
  current_platform () noexcept
  {
    // The usual macro soup. We currently only distinguish x64 Windows and
    // Linux. Everything else gets the generic treatment.
    //
#if defined(_WIN32) || defined(_WIN64)
#  if defined(_M_X64) || defined(__x86_64__)
    return platform_type::windows_x64;
#  else
    return platform_type::unknown;
#  endif
#elif defined(__linux__)
#  if defined(__x86_64__)
    return platform_type::linux_x64;
#  else
    return platform_type::unknown;
#  endif
#else
    return platform_type::unknown;
#endif
  }
}
