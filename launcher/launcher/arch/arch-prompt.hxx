// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <launcher/arch/arch-types.hxx>

namespace launcher
{
  // Return true if both stdin and stdout are terminals (consoles on Windows),
  // that is, if we can interact with the user.
  //
  bool
  interactive () noexcept;

  // Ask the user which client to launch and return the answer. Return
  // nullopt if we are not interactive or if the user cancels the prompt.
  //
  std::optional<architecture>
  prompt_architecture ();
}
