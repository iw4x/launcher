#include <launcher/launcher-shortcut.hxx>

#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <launcher/launcher-log.hxx>

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

    const char* artwork_dir = "cache/shortcut";

    // Labels the shortcuts were installed under before the clients were
    // renamed to IW4x and IW4x (mm).
    //
    vector<string>
    legacy_labels (architecture a)
    {
      switch (a)
      {
        case architecture::x86: return {"IW4x (x86)"};
        case architecture::x64: return {"IW4x (x64)"};
      }

      return {};
    }

    void
    write_image (const fs::path& p, artwork::image i)
    {
      {
        ifstream is (p, ios::binary);

        if (is)
        {
          string c ((istreambuf_iterator<char> (is)),
                    istreambuf_iterator<char> ());

          if (c.size () == i.size () &&
              memcmp (c.data (), i.data (), i.size ()) == 0)
            return;
        }
      }

      error_code ec;

      fs::create_directories (p.parent_path (), ec);

      if (ec)
        throw system_error (ec,
                            "failed to create artwork directory: " +
                              p.parent_path ().string ());

      fs::path t (p);
      t += ".new";

      {
        ofstream os (t, ios::binary | ios::trunc);

        if (!os)
          throw runtime_error ("failed to create " + t.string ());

        os.write (reinterpret_cast<const char*> (i.data ()),
                  static_cast<streamsize> (i.size ()));
        os.flush ();

        if (!os)
          throw runtime_error ("failed to write " + t.string ());
      }

      fs::rename (t, p, ec);

      if (ec)
      {
        error_code ic;
        fs::remove (t, ic);

        throw system_error (ec, "failed to install " + p.string ());
      }
    }
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
    s.legacy_names = legacy_labels (a);

    s.target = launcher_;
    s.arguments = {"--arch", string (to_string (a))};
    s.working_directory = root_;

    fs::path d (root_ / artwork_dir);

    s.image = d / (s.id + "-icon.png");
    s.library.cover = d / (s.id + "-cover.png");
    s.library.wide = d / (s.id + "-wide.png");
    s.library.hero = d / (s.id + "-hero.png");
    s.library.logo = d / (s.id + "-logo.png");

#ifdef _WIN32
    s.icon = root_ / fs::path (architecture_executable (a));
#else
    s.icon = s.image;
#endif

    return s;
  }

  void shortcut_coordinator::
  refresh () noexcept
  {
    vector<shortcut_spec> ss;
    ss.reserve (size (architectures));

    for (architecture a : architectures)
    {
      shortcut_spec s (spec (a));

      try
      {
        const artwork& w (artwork_for (a));

        write_image (s.image, w.icon);
        write_image (s.library.cover, w.cover);
        write_image (s.library.wide, w.wide);
        write_image (s.library.hero, w.hero);
        write_image (s.library.logo, w.logo);
      }
      catch (const exception& e)
      {
        warning ("failed to write artwork for {}: {}", s.name, e.what ());
      }

      ss.push_back (std::move (s));
    }

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
