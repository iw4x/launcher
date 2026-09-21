#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <launcher/shortcut/shortcut-types.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  enum class steam_status
  {
    updated,
    restarted,
    up_to_date,
    not_installed,
    no_users,
    running,
    unreadable
  };

  std::string_view
  to_string (steam_status) noexcept;

  std::ostream&
  operator<< (std::ostream&, steam_status);

  class steam_shortcuts
  {
  public:
    static std::optional<fs::path>
    root ();

    static bool
    running ();

    static std::vector<fs::path>
    lists (const fs::path& root);

    static steam_status
    write (const std::vector<shortcut_spec>&);

    static bool
    shutdown (const fs::path& root);

    static bool
    launch (const fs::path& root);

    static std::int32_t
    app_id (const std::string& exe, const std::string& name);
  };
}
