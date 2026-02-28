#pragma once

#include <quill/core/LogLevel.h>

namespace launcher
{
  namespace log
  {
    // Minimum severity level.
    //
    // The idea here is that every log statement whose severity falls strictly
    // below this threshold is to be removed by the compiler entirely. That is,
    // the threshold is a compile-time constant so the optimizer can fold the
    // guarding if constexpr in each dispatch struct to a no-op.
    //
    // Note that development builds open the full trace range.
    //
  #if LAUNCHER_DEVELOP
    inline constexpr quill::LogLevel
      compiled_minimum_level (quill::LogLevel::TraceL3);
#else
    // TraceL3 for beta, change on release
    //
    inline constexpr quill::LogLevel
      compiled_minimum_level (quill::LogLevel::TraceL3);
#endif
  }
}
