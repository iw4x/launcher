#include <hello/steam/steam-types.hxx>

#include <cassert>
#include <stdexcept>

namespace hello
{
  std::string
  to_string (steam_error e)
  {
    switch (e)
    {
      case steam_error::none: return "none";
      case steam_error::steam_not_found: return "steam not found";
      case steam_error::config_not_found: return "config not found";
      case steam_error::library_not_found: return "library not found";
      case steam_error::app_not_found: return "app not found";
      case steam_error::parse_error: return "parse error";
      case steam_error::invalid_path: return "invalid path";
      case steam_error::permission_denied: return "permission denied";
    }

    assert (false);
    throw std::invalid_argument ("invalid steam_error value");
  }

  std::string
  to_string (vdf_value_type t)
  {
    switch (t)
    {
      case vdf_value_type::string: return "string";
      case vdf_value_type::object: return "object";
    }

    assert (false);
    throw std::invalid_argument ("invalid vdf_value_type value");
  }
}
