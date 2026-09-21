#include <launcher/launcher-shortcut.hxx>

#include <exception>
#include <string>
#include <utility>
#include <vector>

#include <launcher/launcher-log.hxx>
#include <launcher/launcher-manifest.hxx>

using namespace std;

namespace launcher
{
  namespace
  {
    constexpr auto
    info ([] (auto&&... a)
    {
      log::info (categories::shortcut (), std::forward<decltype (a)> (a)...);
    });

    constexpr auto
    warning ([] (auto&&... a)
    {
      log::warning (categories::shortcut (),
                    std::forward<decltype (a)> (a)...);
    });

    constexpr auto
    trace_l2 ([] (auto&&... a)
    {
      log::trace_l2 (categories::shortcut (),
                     std::forward<decltype (a)> (a)...);
    });

    constexpr shortcut_scope scopes[] =
    {
      shortcut_scope::desktop,
      shortcut_scope::menu
    };
  }

  shortcut_coordinator::
  shortcut_coordinator (fs::path l, fs::path r)
    : launcher_ (std::move (l)),
      root_ (std::move (r))
  {
  }

  shortcut_spec shortcut_coordinator::
  spec (architecture a) const
  {
    shortcut_spec s;

    s.id = "iw4x-" + string (to_string (a));
    s.name = architecture_label (a);
    s.comment = architecture_description (a);

    s.target = launcher_;
    s.arguments = {"--arch", string (to_string (a))};
    s.working_directory = root_;

#ifdef _WIN32
    s.icon = root_ / fs::path (architecture_executable (a));
#else
    manifest_file f;
    f.path = "iw4x/images/icon.png";

    s.icon = manifest_coordinator::resolve_path (f, root_);
#endif

    return s;
  }

  void shortcut_coordinator::
  refresh () noexcept
  {
    vector<shortcut_spec> ss;
    ss.reserve (size (architectures));

    for (architecture a : architectures)
      ss.push_back (spec (a));

    for (const shortcut_spec& s : ss)
    {
      for (shortcut_scope c : scopes)
      {
        if (!shortcut_writer::supported (c))
          continue;

        try
        {
          if (shortcut_writer::write (s, c))
            info ("updated {} shortcut for {}", to_string (c), s.name);
          else
            trace_l2 ("{} shortcut for {} is up to date",
                      to_string (c),
                      s.name);
        }
        catch (const exception& e)
        {
          warning ("failed to update {} shortcut for {}: {}",
                   to_string (c),
                   s.name,
                   e.what ());
        }
      }
    }

    try
    {
      steam_status t (steam_shortcuts::write (ss));

      switch (t)
      {
      case steam_status::updated:
        info ("updated steam library entries");
        break;

      case steam_status::restarted:
        info ("updated steam library entries; steam was closed and "
              "reopened, since it writes its own copy of the list out as "
              "it exits and would otherwise have undone the change");
        break;

      case steam_status::running:
        warning ("steam would not close, so its library entries were left "
                 "alone; close steam and run the launcher again to add "
                 "them");
        break;

      case steam_status::unreadable:
        warning ("steam's shortcut list is in a form this launcher does not "
                 "recognize and was left untouched");
        break;

      default:
        trace_l2 ("steam library entries: {}", to_string (t));
        break;
      }
    }
    catch (const exception& e)
    {
      warning ("failed to update steam library entries: {}", e.what ());
    }
  }
}
