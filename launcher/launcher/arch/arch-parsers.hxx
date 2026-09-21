#pragma once

#include <launcher/arch/arch-types.hxx>

namespace launcher
{
  namespace cli
  {
    class scanner;

    template <typename T>
    struct parser;

    template <>
    struct parser<architecture>
    {
      static void
      parse (architecture&, bool&, scanner&);
    };
  }
}
