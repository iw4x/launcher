// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <filesystem>
#include <string_view>

namespace launcher
{
  // Return the path as a UTF-8 string.
  //
  std::string
  to_utf8 (const std::filesystem::path&);

  // Return the path that the UTF-8 string represents.
  //
  std::filesystem::path
  from_utf8 (std::string_view);

  // Return the system error message (error_code::message(),
  // system_error::what()) in UTF-8.
  //
  // Note that on Windows such messages are encoded in the ANSI code page
  // (for example, CP1251 on Russian installs). Elsewhere they are returned
  // as is.
  //
  std::string
  to_utf8_system_message (const std::string&);
}
