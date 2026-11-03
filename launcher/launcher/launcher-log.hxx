// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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
  // Logging setup. The constructor starts the backend and creates the
  // logger of every category, writing to the console and to a file in
  // cache/. The destructor stops the backend.
  //
  // Note that there should be a single instance, which active_logger points
  // to.
  //
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
    // Log statements, one per severity, for example:
    //
    // log::info {categories::update{}, "updating {}", name};
    //
    // We use a class template with a deduction guide since the source
    // location default argument has to come after the format arguments.
    //
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
