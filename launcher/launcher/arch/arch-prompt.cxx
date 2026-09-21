#include <launcher/arch/arch-prompt.hxx>

#include <cstddef>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace std;

namespace launcher
{
  bool
  interactive () noexcept
  {
#ifdef _WIN32
    auto console ([] (DWORD h)
    {
      HANDLE n (GetStdHandle (h));

      if (n == nullptr || n == INVALID_HANDLE_VALUE)
        return false;

      DWORD m;
      return GetConsoleMode (n, &m) != 0;
    });

    return console (STD_INPUT_HANDLE) && console (STD_OUTPUT_HANDLE);
#else
    return isatty (STDIN_FILENO) == 1 && isatty (STDOUT_FILENO) == 1;
#endif
  }

  optional<architecture>
  prompt_architecture ()
  {
    if (!interactive ())
      return nullopt;

    using namespace ftxui;

    vector<string> es;
    es.reserve (size (architectures));

    for (architecture a : architectures)
      es.push_back (string (architecture_label (a)));

    int sel (0);

    ScreenInteractive screen (ScreenInteractive::TerminalOutput ());

    bool ok (false);

    MenuOption mo (MenuOption::Vertical ());

    mo.on_enter = [&screen, &ok] ()
    {
      ok = true;
      screen.Exit ();
    };

    Component menu (Menu (&es, &sel, mo));

    Component view (Renderer (menu, [&menu, &sel] ()
    {
      architecture a (architectures[static_cast<size_t> (sel)]);

      return vbox ({
        text ("Which IW4x client would you like to play?") | bold,
        separator (),
        menu->Render (),
        separator (),
        paragraph (string (architecture_description (a))),
        separator (),
        text ("Up/Down to choose, Enter to launch, Esc to quit") | dim
      }) | border | size (WIDTH, GREATER_THAN, 64);
    }));

    view |= CatchEvent ([&screen] (Event e)
    {
      if (e != Event::Escape)
        return false;

      screen.Exit ();
      return true;
    });

    screen.Loop (view);

    if (!ok)
      return nullopt;

    return architectures[static_cast<size_t> (sel)];
  }
}
