#include <launcher/shortcut/shortcut-artwork.hxx>

using namespace std;

namespace launcher
{
  namespace
  {
    constexpr unsigned char x86_icon_png[] =
    {
#embed "artwork/x86-icon.png"
    };

    constexpr unsigned char x86_cover_png[] =
    {
#embed "artwork/x86-cover.png"
    };

    constexpr unsigned char x86_wide_png[] =
    {
#embed "artwork/x86-wide.png"
    };

    constexpr unsigned char x86_hero_png[] =
    {
#embed "artwork/x86-hero.png"
    };

    constexpr unsigned char x86_logo_png[] =
    {
#embed "artwork/x86-logo.png"
    };

    constexpr unsigned char x64_icon_png[] =
    {
#embed "artwork/x64-icon.png"
    };

    constexpr unsigned char x64_cover_png[] =
    {
#embed "artwork/x64-cover.png"
    };

    constexpr unsigned char x64_wide_png[] =
    {
#embed "artwork/x64-wide.png"
    };

    constexpr unsigned char x64_hero_png[] =
    {
#embed "artwork/x64-hero.png"
    };

    constexpr unsigned char x64_logo_png[] =
    {
#embed "artwork/x64-logo.png"
    };

    constinit const artwork x86_artwork
    {
      x86_icon_png,
      x86_cover_png,
      x86_wide_png,
      x86_hero_png,
      x86_logo_png
    };

    constinit const artwork x64_artwork
    {
      x64_icon_png,
      x64_cover_png,
      x64_wide_png,
      x64_hero_png,
      x64_logo_png
    };
  }

  const artwork&
  artwork_for (architecture a) noexcept
  {
    switch (a)
    {
      case architecture::x86: return x86_artwork;
      case architecture::x64: return x64_artwork;
    }

    return x86_artwork;
  }
}
