// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <ostream>
#include <string_view>

namespace launcher
{
  // Client architecture. The x86 client is IW4x and the x64 client is IW4x
  // (mm).
  //
  enum class architecture
  {
    x86,
    x64
  };

  std::ostream&
  operator<< (std::ostream&, architecture);

  // All the architectures, in the order in which we present them.
  //
  inline constexpr architecture architectures[] =
  {
    architecture::x86,
    architecture::x64
  };

  // Return the canonical architecture name, x86 or x64.
  //
  std::string_view
  to_string (architecture) noexcept;

  // Parse the architecture name, ignoring case. Besides the canonical names,
  // recognize the common aliases (32, i686, amd64, x86_64, etc). Return
  // nullopt if the name is not recognized.
  //
  std::optional<architecture>
  parse_architecture (std::string_view) noexcept;

  // Return the name of the client as we show it to the user.
  //
  std::string_view
  architecture_label (architecture) noexcept;

  // Return the description of the client that we show in the architecture
  // prompt.
  //
  std::string_view
  architecture_description (architecture) noexcept;

  // Return the file name of the client executable.
  //
  std::string_view
  architecture_executable (architecture) noexcept;
}
