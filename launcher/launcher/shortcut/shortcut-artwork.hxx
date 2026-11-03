// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>

#include <launcher/arch/arch-types.hxx>

namespace launcher
{
  // Shortcut images embedded into the launcher (generated from
  // artwork/*.png by the buildfile). Note that the shortcuts refer to the
  // copies that the launcher writes into the installation (see
  // shortcut_spec).
  //
  struct artwork
  {
    using image = std::span<const unsigned char>;

    image icon;
    image cover;
    image wide;
    image hero;
    image logo;
  };

  // Return the images for the architecture.
  //
  const artwork&
  artwork_for (architecture) noexcept;
}
