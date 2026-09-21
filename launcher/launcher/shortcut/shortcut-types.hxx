#pragma once

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace launcher
{
  namespace fs = std::filesystem;

  enum class shortcut_scope
  {
    desktop,
    menu
  };

  std::string_view
  to_string (shortcut_scope) noexcept;

  std::ostream&
  operator<< (std::ostream&, shortcut_scope);

  struct shortcut_spec
  {
    std::string id;
    std::string name;
    std::string comment;

    fs::path target;
    std::vector<std::string> arguments;
    fs::path working_directory;

    fs::path icon;
    fs::path image;

    struct library_artwork
    {
      fs::path cover;
      fs::path wide;
      fs::path hero;
      fs::path logo;
    };

    library_artwork library;
  };
}
