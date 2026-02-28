#include <launcher/update/update-types.hxx>

#include <cctype>
#include <sstream>

#include <launcher/launcher-log.hxx>

using namespace std;

namespace launcher
{
  // Parse a distinct unsigned integer. Return nullopt if we are at the end or
  // looking at garbage.
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

  // Check if the current char matches c and advance if it does.
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
    // Standard semver precedence for the main components.
    //
    if (major != v.major) return major < v.major ? -1 : 1;
    if (minor != v.minor) return minor < v.minor ? -1 : 1;
    if (patch != v.patch) return patch < v.patch ? -1 : 1;

    // Now it gets tricky. We store pre-release as a unit16_t where 0 means
    // "final release", but in semver, 1.0.0 is greater than 1.0.0-alpha.
    //
    bool tr (pre_release == 0 && snapshot_sn == 0);
    bool vr (v.pre_release == 0 && v.snapshot_sn == 0);

    // If one is a release and the other isn't, the release wins.
    //
    if (tr != vr)
      return tr ? 1 : -1;

    // Both are pre-releases (or both are releases). Since we map alpha to low
    // numbers (1+) and beta to high numbers (500+), simple integer comparison
    // works here.
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

    // Map our internal representation back to string. Recall that we offset
    // betas by 500 to keep the sort order sane in the integer member.
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
      return v; // Return what we have so far
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

    // Map the pre-release number into our linearized space.
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

    // Build2 uses ".z" as a development snapshot placeholder (e.g.,
    // "1.2.0-a.1.z"). Treat it as a minimal snapshot indicator so the version
    // sorts correctly.
    //
    if (p < s.size () && (s[p] == 'z' || s[p] == 'Z'))
    {
      ++p;
      v.snapshot_sn = 1; // any non-zero value to mark it as a snapshot
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

    // If there is anything left after the dot, treat it as the snapshot ID.
    // We stop at typical delimiters just in case.
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
