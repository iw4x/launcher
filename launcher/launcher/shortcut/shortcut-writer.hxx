#pragma once

#include <filesystem>
#include <optional>

#include <launcher/shortcut/shortcut-types.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  class shortcut_writer
  {
  public:
    static bool
    supported (shortcut_scope) noexcept;

    static std::optional<fs::path>
    directory (shortcut_scope);

    static std::optional<fs::path>
    location (const shortcut_spec&, shortcut_scope);

    static bool
    write (const shortcut_spec&, shortcut_scope);
  };
}
