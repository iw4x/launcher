// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <quill/core/LogLevel.h>

namespace launcher
{
  namespace log
  {
    // Lowest severity that we compile log statements for. Statements below
    // it compile to nothing (see LAUNCHER_LOG_SEVERITY in launcher-log.hxx).
    //
    // We compile in the whole trace range so that it only takes lowering the
    // run-time threshold to get the trace, as the development build does
    // (see launcher-log.cxx).
    //
    inline constexpr quill::LogLevel
      compiled_minimum_level (quill::LogLevel::TraceL3);
  }
}
