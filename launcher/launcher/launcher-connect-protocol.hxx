// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>

namespace launcher
{
  // Register the executable as the handler of the iw4x URL scheme for the
  // current user, which is how iw4x:// links start the launcher.
  // Only Windows has such a registration and elsewhere we do nothing. We
  // log a failure and otherwise ignore it.
  //
  void
  refresh_connect_protocol (const std::filesystem::path&) noexcept;
}
