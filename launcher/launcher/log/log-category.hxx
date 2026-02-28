#pragma once

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/core/PatternFormatterOptions.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/RotatingFileSink.h>

#include <concepts>
#include <string_view>

namespace launcher
{
  namespace categories
  {
    struct launcher {};
    struct cache {};
    struct download {};
    struct github {};
    struct http {};
    struct manifest {};
    struct progress {};
    struct steam {};
    struct update {};
  }

  namespace log
  {
    template <typename C>
    struct policy;

    template <>
    struct policy<categories::launcher>
    {
      static constexpr std::string_view name      = "launcher";
      static constexpr quill::LogLevel  threshold = quill::LogLevel::Info;
    };
    template <>
    struct policy<categories::cache>
    {
      static constexpr std::string_view name      = "cache";
      static constexpr quill::LogLevel  threshold = quill::LogLevel::Info;
    };

    template <>
    struct policy<categories::download>
    {
      static constexpr std::string_view name      = "download";
      static constexpr quill::LogLevel  threshold = quill::LogLevel::Info;
    };

    template <>
    struct policy<categories::github>
    {
      static constexpr std::string_view name      = "github";
      static constexpr quill::LogLevel  threshold = quill::LogLevel::Info;
    };

    template <>
    struct policy<categories::http>
    {
      static constexpr std::string_view name      = "http";
      static constexpr quill::LogLevel  threshold = quill::LogLevel::Info;
    };

    template <>
    struct policy<categories::manifest>
    {
      static constexpr std::string_view name      = "manifest";
      static constexpr quill::LogLevel  threshold = quill::LogLevel::Info;
    };

    template <>
    struct policy<categories::progress>
    {
      static constexpr std::string_view name      = "progress";
      static constexpr quill::LogLevel  threshold = quill::LogLevel::Info;
    };

    template <>
    struct policy<categories::steam>
    {
      static constexpr std::string_view name      = "steam";
      static constexpr quill::LogLevel  threshold = quill::LogLevel::Info;
    };

    template <>
    struct policy<categories::update>
    {
      static constexpr std::string_view name      = "update";
      static constexpr quill::LogLevel  threshold = quill::LogLevel::Info;
    };

    template <typename C>
    concept Category = requires
    {
      { policy<C>::name      } -> std::convertible_to<std::string_view>;
      { policy<C>::threshold } -> std::convertible_to<quill::LogLevel>;
    };

    namespace detail
    {
      template <typename C>
      quill::Logger*&
      logger () noexcept
      {
        static quill::Logger* p (nullptr);
        return p;
      }
    }

    template <typename C>
    quill::Logger*
    logger () noexcept
    {
      return detail::logger<C> ();
    }
  }
}
