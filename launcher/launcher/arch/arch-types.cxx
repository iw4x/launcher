#include <launcher/arch/arch-types.hxx>

#include <algorithm>
#include <cctype>
#include <string>

using namespace std;

namespace launcher
{
  namespace
  {
    string
    fold (string_view s)
    {
      string r (s);

      transform (r.begin (), r.end (), r.begin (), [] (unsigned char c)
      {
        return static_cast<char> (tolower (c));
      });

      return r;
    }
  }

  ostream&
  operator<< (ostream& o, architecture a)
  {
    return o << to_string (a);
  }

  string_view
  to_string (architecture a) noexcept
  {
    switch (a)
    {
      case architecture::x86: return "x86";
      case architecture::x64: return "x64";
    }

    return "x86";
  }

  optional<architecture>
  parse_architecture (string_view s) noexcept
  {
    string v (fold (s));

    if (v == "x86" || v == "32" || v == "i386" || v == "i686" || v == "win32")
      return architecture::x86;

    if (v == "x64" || v == "64" || v == "amd64" || v == "x86_64" ||
        v == "x86-64" || v == "win64")
      return architecture::x64;

    return nullopt;
  }

  string_view
  architecture_label (architecture a) noexcept
  {
    switch (a)
    {
      case architecture::x86: return "IW4x (x86)";
      case architecture::x64: return "IW4x (x64)";
    }

    return "IW4x";
  }

  string_view
  architecture_description (architecture a) noexcept
  {
    switch (a)
    {
    case architecture::x86:
      return "IW4x (x86) is built around community-hosted dedicated servers. "
             "Each server can have its own rules and mods.";

    case architecture::x64:
      return "IW4x (x64) is built around matchmaking, with friends and "
             "parties available directly in the client.";
    }

    return "";
  }

  string_view
  architecture_executable (architecture a) noexcept
  {
    switch (a)
    {
      case architecture::x86: return "iw4x (x86).exe";
      case architecture::x64: return "iw4x (x64).exe";
    }

    return "iw4x (x86).exe";
  }
}
