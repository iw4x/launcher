// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

// Definitions that keep the launcher within the glibc floor.
//
// The Linux launcher that we ship runs on any host, so it can only depend
// on what the oldest supported glibc provides. This floor is glibc 2.36,
// which is the version in Debian 12, the oldest Debian release with
// long-term support. For our own code glibc.hxx (which every other
// translation unit of the launcher includes first) binds each reference to
// a version available at the floor.
//
// But some references don't have such a version. The glibc 2.38 and later
// headers redirect several C library functions to their C23 variants, which
// only exist in those versions. And the statically linked C++ runtime
// (libstdc++.a and libgcc_eh.a) and dependencies are compiled without our
// bindings, so their references bind to the latest versions in the build
// host's glibc. So here we define each symbol that the launcher references
// and the floor lacks. These definitions resolve the references at link
// time and require nothing from glibc at runtime beyond the floor.
//
// Some of these symbols are the same functions under a newer version, which
// glibc.hxx binds to their older version. So this translation unit doesn't
// include it and instead binds the few references that it makes itself.
//
// If the launcher requires anything else, then its floor test fails (see
// glibc/testscript in launcher-tests) and the missing definition should be
// added here.
//
#include <cctype> // isspace()
#include <cerrno>
#include <cstdarg>
#include <limits>

using namespace std;

// The floor versions of the functions that we forward to, under their own
// names. Note that the C17 integer conversion functions are the unversioned
// ones, which the headers of glibc 2.38 and later redirect to their C23
// variants (defined below).
//
extern "C"
{
  unsigned long long
  c17_strtoull (const char*, char**, int) noexcept __asm__ ("strtoull");

  unsigned long
  c17_strtoul (const char*, char**, int) noexcept __asm__ ("strtoul");

  long long
  c17_strtoll (const char*, char**, int) noexcept __asm__ ("strtoll");

  long
  c17_strtol (const char*, char**, int) noexcept __asm__ ("strtol");

  int
  c99_vsscanf (const char*, const char*, va_list) noexcept
    __asm__ ("__isoc99_vsscanf");

  double
  floor_fmod (double, double) noexcept __asm__ ("floor_fmod");

  float
  floor_sqrtf (float) noexcept __asm__ ("floor_sqrtf");
}

__asm__ (".symver floor_fmod,fmod@GLIBC_2.2.5");
__asm__ (".symver floor_sqrtf,sqrtf@GLIBC_2.2.5");

namespace
{
  // C23 integer conversion, which additionally accepts the 0b and 0B binary
  // prefixes. The prefix is recognized for the bases 0 and 2 when followed
  // by a binary digit. In all the other cases the result matches C17, which
  // parses the prefix's 0 as the number and stops at the b.
  //
  // Note that the callers are the C++ runtime and the dependencies, so we
  // treat the arguments the same as glibc does and don't check them.
  //
  template <typename T>
  T
  c23_strto (const char* s,
             char** e,
             int b,
             T (*c17) (const char*, char**, int) noexcept) noexcept
  {
    if (b == 0 || b == 2)
    {
      const char* p (s);

      while (isspace (static_cast<unsigned char> (*p)))
        ++p;

      bool minus (*p == '-');

      if (*p == '-' || *p == '+')
        ++p;

      if (p[0] == '0'                   &&
          (p[1] == 'b' || p[1] == 'B') &&
          (p[2] == '0' || p[2] == '1'))
      {
        // The digits after the prefix cannot have their own sign or
        // whitespace, so parse their magnitude with C17 strtoull() in base 2
        // and apply the sign and the range of the result type the same as
        // C17 does: a signed result saturates in the direction of the sign
        // and an unsigned one is negated unless the magnitude is out of
        // range.
        //
        using U = unsigned long long;
        using L = numeric_limits<T>;

        int x (errno);

        errno = 0;
        U v (c17_strtoull (p + 2, e, 2));
        bool r (errno == ERANGE);
        errno = x;

        if constexpr (L::is_signed)
        {
          U m (minus
               ? static_cast<U> (L::max ()) + 1
               : static_cast<U> (L::max ()));

          if (r || v > m)
          {
            errno = ERANGE;
            return minus ? L::min () : L::max ();
          }

          // Negate the magnitude in the unsigned type, where the minimum's
          // magnitude doesn't overflow, and convert the result back.
          //
          return static_cast<T> (minus ? 0 - v : v);
        }
        else
        {
          if (r || v > L::max ())
          {
            errno = ERANGE;
            return L::max ();
          }

          T t (static_cast<T> (v));
          return minus ? 0 - t : t;
        }
      }
    }

    return c17 (s, e, b);
  }
}

extern "C"
{
  // C23 integer conversion functions (glibc 2.38 and later). libstdc++ uses
  // __isoc23_strtoul() to parse GLIBCXX_TUNABLES (eh_alloc.cc and debug.cc).
  //
  long
  __isoc23_strtol (const char* s, char** e, int b) noexcept
  {
    return c23_strto (s, e, b, &c17_strtol);
  }

  long long
  __isoc23_strtoll (const char* s, char** e, int b) noexcept
  {
    return c23_strto (s, e, b, &c17_strtoll);
  }

  unsigned long
  __isoc23_strtoul (const char* s, char** e, int b) noexcept
  {
    return c23_strto (s, e, b, &c17_strtoul);
  }

  unsigned long long
  __isoc23_strtoull (const char* s, char** e, int b) noexcept
  {
    return c23_strto (s, e, b, &c17_strtoull);
  }

  // C23 sscanf() (glibc 2.38 and later), which differs from C99 sscanf() in
  // the %b conversion and the 0b prefix of the %i conversion. libcrypto and
  // Boost.Filesystem use neither, so we forward to the C99 variant.
  //
  int
  __isoc23_sscanf (const char* s, const char* f, ...) noexcept
  {
    va_list a;
    va_start (a, f);
    int r (c99_vsscanf (s, f, a));
    va_end (a);
    return r;
  }

  // The glibc 2.38 fmod() and the glibc 2.43 sqrtf() are new versions of the
  // functions without the System V error handling wrapper. The earlier
  // versions keep the wrapper, which behaves the same since glibc 2.27
  // removed the means to select the System V error handling. So we forward
  // to them. SQLite uses fmod() and FTXUI sqrtf().
  //
  double
  fmod (double x, double y) noexcept
  {
    return floor_fmod (x, y);
  }

  float
  sqrtf (float x) noexcept
  {
    return floor_sqrtf (x);
  }
}
