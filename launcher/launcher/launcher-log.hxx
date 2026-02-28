#pragma once

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogFunctions.h>
#include <quill/Logger.h>

#include <quill/backend/BackendUtilities.h>

#include <quill/core/LogLevel.h>
#include <quill/core/SourceLocation.h>

#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/RotatingFileSink.h>

#include <launcher/log/log-category.hxx>
#include <launcher/log/log-severity.hxx>

namespace launcher
{
  class logger
  {
  public:
    logger ();
    ~logger ();

    logger (const logger&) = delete;
    logger& operator = (const logger&) = delete;

    logger (logger&&) = delete;
    logger& operator = (logger&&) = delete;
  };

  extern logger* active_logger;

  namespace log
  {
    #define LAUNCHER_LOG_SEVERITY(N, L)                                            \
    template <Category C, typename... A>                                       \
    struct N                                                                   \
    {                                                                          \
      N (C,                                                                    \
         char const* f,                                                        \
         A&&... a,                                                             \
         quill::SourceLocation l = quill::SourceLocation::current ())          \
      {                                                                        \
        if constexpr (L >= compiled_minimum_level)                             \
        {                                                                      \
          quill::Logger* q (logger<C> ());                                     \
                                                                               \
          if (q && q->should_log_statement (L))                                \
          {                                                                    \
            quill::log (q, "", L, f, l, static_cast<A&&> (a)...);              \
          }                                                                    \
        }                                                                      \
      }                                                                        \
    };                                                                         \
                                                                               \
    template <Category C, typename... A>                                       \
    N (C, char const*, A&&...) -> N<C, A...>

    LAUNCHER_LOG_SEVERITY (trace_l3, quill::LogLevel::TraceL3);
    LAUNCHER_LOG_SEVERITY (trace_l2, quill::LogLevel::TraceL2);
    LAUNCHER_LOG_SEVERITY (trace_l1, quill::LogLevel::TraceL1);
    LAUNCHER_LOG_SEVERITY (debug,    quill::LogLevel::Debug);
    LAUNCHER_LOG_SEVERITY (info,     quill::LogLevel::Info);
    LAUNCHER_LOG_SEVERITY (notice,   quill::LogLevel::Notice);
    LAUNCHER_LOG_SEVERITY (warning,  quill::LogLevel::Warning);
    LAUNCHER_LOG_SEVERITY (error,    quill::LogLevel::Error);
    LAUNCHER_LOG_SEVERITY (critical, quill::LogLevel::Critical);
    #undef LAUNCHER_LOG_SEVERITY
  }
}
