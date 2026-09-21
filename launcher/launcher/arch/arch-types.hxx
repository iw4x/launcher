#pragma once

#include <optional>
#include <ostream>
#include <string_view>

namespace launcher
{
  enum class architecture
  {
    x86,
    x64
  };

  std::ostream&
  operator<< (std::ostream&, architecture);

  inline constexpr architecture architectures[] =
  {
    architecture::x86,
    architecture::x64
  };

  std::string_view
  to_string (architecture) noexcept;

  std::optional<architecture>
  parse_architecture (std::string_view) noexcept;

  std::string_view
  architecture_label (architecture) noexcept;

  std::string_view
  architecture_description (architecture) noexcept;

  std::string_view
  architecture_executable (architecture) noexcept;
}
