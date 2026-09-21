#pragma once

#include <optional>

#include <launcher/arch/arch-types.hxx>

namespace launcher
{
  bool
  interactive () noexcept;

  std::optional<architecture>
  prompt_architecture ();
}
