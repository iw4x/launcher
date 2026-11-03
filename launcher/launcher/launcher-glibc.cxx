// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

// Definitions that keep the launcher within the glibc floor.
//
// The Linux launcher that we ship runs on any host, so it can only depend
// on what the oldest supported glibc provides. This floor is glibc 2.36,
// which is the version in Debian 12, the oldest Debian release with
// long-term support. For our own code launcher-glibc.hxx (which every other
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
// launcher-glibc.hxx binds to their older version. So this translation unit
// doesn't include it and instead binds the few references that it makes
// itself.
//
// If the launcher requires anything else, then its floor test fails (see
// glibc/testscript in launcher-tests) and the missing definition should be
// added here.
//
#include <cerrno>
#include <cstdarg>
#include <limits>
#include <cwctype>  // iswspace()
#include <concepts>

#include <ctype.h>  // isspace(), isspace_l()
#include <locale.h> // locale_t

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

  unsigned long long
  c17_strtoull_l (const char*, char**, int, locale_t) noexcept
    __asm__ ("strtoull_l");

  long long
  c17_strtoll_l (const char*, char**, int, locale_t) noexcept
    __asm__ ("strtoll_l");

  unsigned long long
  c17_wcstoull (const wchar_t*, wchar_t**, int) noexcept __asm__ ("wcstoull");

  unsigned long
  c17_wcstoul (const wchar_t*, wchar_t**, int) noexcept __asm__ ("wcstoul");

  long long
  c17_wcstoll (const wchar_t*, wchar_t**, int) noexcept __asm__ ("wcstoll");

  long
  c17_wcstol (const wchar_t*, wchar_t**, int) noexcept __asm__ ("wcstol");

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

// Keep our fmod() and sqrtf() (defined below) out of the dynamic symbol
// table. The linker exports a definition that a shared library also
// defines, which here would be next to the floor versions of the same names
// that the definitions reference. And binutils 2.45 ld then writes a corrupt
// dynamic symbol table entry, which objdump and readelf reject. Note that we
// cannot use the visibility attribute since GCC ignores it on the definition
// of a builtin function.
//
__asm__ (".hidden fmod");
__asm__ (".hidden sqrtf");

namespace
{
  // Character type of the C integer conversion functions.
  //
  template <typename C>
  concept conversion_char = same_as<C, char> || same_as<C, wchar_t>;

  // C17 integer conversion, a function or a function object that binds the
  // locale of a locale variant (see locale_c17 below).
  //
  template <typename F, typename C, typename T>
  concept c17_conversion =
    conversion_char<C> &&
    integral<T>        &&
    requires (const F& f, const C* s, C** e, int b)
    {
      {f (s, e, b)} -> same_as<T>;
    };

  // C23 integer conversion, which additionally accepts the 0b and 0B binary
  // prefixes. The prefix is recognized for the bases 0 and 2 when followed
  // by a binary digit. In all the other cases the result matches C17, which
  // parses the prefix's 0 as the number and stops at the b.
  //
  // The space predicate classifies the leading whitespace, c17 converts in
  // all the other cases, and c17_ull converts the binary digits after the
  // prefix. They differ between the narrow, wide, and locale variants.
  //
  // Note that the callers are the C++ runtime and the dependencies, so we
  // treat the arguments the same as glibc does and don't check them. Note
  // also that the result type cannot be deduced, so the callers specify it
  // explicitly.
  //
  template <integral T,
            conversion_char C,
            predicate<C> S,
            c17_conversion<C, T> F,
            c17_conversion<C, unsigned long long> U>
  T
  c23_strto (const C* s,
             C** e,
             int b,
             const S& space,
             const F& c17,
             const U& c17_ull) noexcept
  {
    if (b == 0 || b == 2)
    {
      const C* p (s);

      while (space (*p))
        ++p;

      const bool minus (*p == '-');

      if (*p == '-' || *p == '+')
        ++p;

      if (p[0] == '0'                  &&
          (p[1] == 'b' || p[1] == 'B') &&
          (p[2] == '0' || p[2] == '1'))
      {
        // The digits after the prefix cannot have their own sign or
        // whitespace, so parse their magnitude with the C17 unsigned
        // conversion in base 2 and apply the sign and the range of the result
        // type the same as C17 does: a signed result saturates in the
        // direction of the sign and an unsigned one is negated unless the
        // magnitude is out of range.
        //
        using magnitude = unsigned long long;
        using limits = numeric_limits<T>;

        const int x (errno);

        errno = 0;
        const magnitude v (c17_ull (p + 2, e, 2));
        const bool r (errno == ERANGE);
        errno = x;

        if constexpr (limits::is_signed)
        {
          const magnitude m (minus
                             ? static_cast<magnitude> (limits::max ()) + 1
                             : static_cast<magnitude> (limits::max ()));

          if (r || v > m)
          {
            errno = ERANGE;
            return minus ? limits::min () : limits::max ();
          }

          // Negate the magnitude in the unsigned type, where the minimum's
          // magnitude doesn't overflow, and convert the result back.
          //
          return static_cast<T> (minus ? 0 - v : v);
        }
        else
        {
          if (r || v > limits::max ())
          {
            errno = ERANGE;
            return limits::max ();
          }

          const T t (static_cast<T> (v));
          return minus ? 0 - t : t;
        }
      }
    }

    return c17 (s, e, b);
  }

  // Whitespace predicates of the narrow and wide conversions.
  //
  bool
  narrow_space (char c) noexcept
  {
    return isspace (static_cast<unsigned char> (c)) != 0;
  }

  bool
  wide_space (wchar_t c) noexcept
  {
    return iswspace (static_cast<wint_t> (c)) != 0;
  }

  // Whitespace predicate and C17 conversion of the locale variants. They
  // bind the locale, so that c23_strto() calls them the same as the others.
  //
  struct locale_space
  {
    locale_t locale;

    bool
    operator() (char c) const noexcept
    {
      return isspace_l (static_cast<unsigned char> (c), locale) != 0;
    }
  };

  template <integral T>
  struct locale_c17
  {
    T (*function) (const char*, char**, int, locale_t) noexcept;
    locale_t locale;

    T
    operator() (const char* s, char** e, int b) const noexcept
    {
      return function (s, e, b, locale);
    }
  };
}

extern "C"
{
  // C23 integer conversion functions (glibc 2.38 and later). libstdc++ uses
  // __isoc23_strtoul() to parse GLIBCXX_TUNABLES (eh_alloc.cc and debug.cc).
  //
  long
  __isoc23_strtol (const char* s, char** e, int b) noexcept
  {
    return c23_strto<long> (s, e, b,
                            narrow_space,
                            c17_strtol,
                            c17_strtoull);
  }

  long long
  __isoc23_strtoll (const char* s, char** e, int b) noexcept
  {
    return c23_strto<long long> (s, e, b,
                                 narrow_space,
                                 c17_strtoll,
                                 c17_strtoull);
  }

  unsigned long
  __isoc23_strtoul (const char* s, char** e, int b) noexcept
  {
    return c23_strto<unsigned long> (s, e, b,
                                     narrow_space,
                                     c17_strtoul,
                                     c17_strtoull);
  }

  unsigned long long
  __isoc23_strtoull (const char* s, char** e, int b) noexcept
  {
    return c23_strto<unsigned long long> (s, e, b,
                                          narrow_space,
                                          c17_strtoull,
                                          c17_strtoull);
  }

  // The locale variants (glibc 2.38 and later), which libc++ uses for
  // parsing numbers (std::num_get). They classify the whitespace in the
  // specified locale.
  //
  long long
  __isoc23_strtoll_l (const char* s, char** e, int b, locale_t l) noexcept
  {
    const locale_space space {l};
    const locale_c17 c17 {c17_strtoll_l, l};
    const locale_c17 c17_ull {c17_strtoull_l, l};

    return c23_strto<long long> (s, e, b, space, c17, c17_ull);
  }

  unsigned long long
  __isoc23_strtoull_l (const char* s, char** e, int b, locale_t l) noexcept
  {
    const locale_space space {l};
    const locale_c17 c17 {c17_strtoull_l, l};
    const locale_c17 c17_ull {c17_strtoull_l, l};

    return c23_strto<unsigned long long> (s, e, b, space, c17, c17_ull);
  }

  // The wide variants (glibc 2.38 and later), which libc++ uses for
  // std::wstring conversions (std::stol(), etc).
  //
  long
  __isoc23_wcstol (const wchar_t* s, wchar_t** e, int b) noexcept
  {
    return c23_strto<long> (s, e, b,
                            wide_space,
                            c17_wcstol,
                            c17_wcstoull);
  }

  long long
  __isoc23_wcstoll (const wchar_t* s, wchar_t** e, int b) noexcept
  {
    return c23_strto<long long> (s, e, b,
                                 wide_space,
                                 c17_wcstoll,
                                 c17_wcstoull);
  }

  unsigned long
  __isoc23_wcstoul (const wchar_t* s, wchar_t** e, int b) noexcept
  {
    return c23_strto<unsigned long> (s, e, b,
                                     wide_space,
                                     c17_wcstoul,
                                     c17_wcstoull);
  }

  unsigned long long
  __isoc23_wcstoull (const wchar_t* s, wchar_t** e, int b) noexcept
  {
    return c23_strto<unsigned long long> (s, e, b,
                                          wide_space,
                                          c17_wcstoull,
                                          c17_wcstoull);
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

  // C23 vsscanf() (glibc 2.38 and later), which libc++ uses for parsing
  // floating-point numbers (std::num_get). As above, these don't use the %b
  // conversion or the 0b prefix.
  //
  int
  __isoc23_vsscanf (const char* s, const char* f, va_list a) noexcept
  {
    return c99_vsscanf (s, f, a);
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
