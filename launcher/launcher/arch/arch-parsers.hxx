// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <launcher/arch/arch-types.hxx>

namespace launcher
{
  namespace cli
  {
    class scanner;

    template <typename T>
    struct parser;

    // Parse the --arch option value (see launcher.cli). Throw missing_value
    // if there is no value and invalid_value if it is not an architecture
    // name (see parse_architecture()).
    //
    template <>
    struct parser<architecture>
    {
      static void
      parse (architecture&, bool&, scanner&);
    };
  }
}
