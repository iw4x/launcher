#pragma once

#include <span>

#include <launcher/arch/arch-types.hxx>

namespace launcher
{
  struct artwork
  {
    using image = std::span<const unsigned char>;

    image icon;
    image cover;
    image wide;
    image hero;
    image logo;
  };

  const artwork&
  artwork_for (architecture) noexcept;
}
