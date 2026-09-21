#include <launcher/shortcut/shortcut-types.hxx>

using namespace std;

namespace launcher
{
  string_view
  to_string (shortcut_scope s) noexcept
  {
    switch (s)
    {
      case shortcut_scope::desktop: return "desktop";
      case shortcut_scope::menu:    return "menu";
    }

    return "";
  }

  ostream&
  operator<< (ostream& o, shortcut_scope s)
  {
    return o << to_string (s);
  }
}
