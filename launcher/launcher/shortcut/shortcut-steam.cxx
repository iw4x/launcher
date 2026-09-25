#include <launcher/shortcut/shortcut-steam.hxx>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <thread>

#include <boost/process.hpp>
#include <boost/process/v1/extend.hpp>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <cerrno>
#  include <signal.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

#include <miniz.h>

#include <launcher/shortcut/shortcut-vdf.hxx>

using namespace std;

namespace launcher
{
  namespace
  {
    constexpr const char* shortcuts_key = "shortcuts";

    constexpr const char* field_app_id  = "appid";
    constexpr const char* field_name    = "AppName";
    constexpr const char* field_exe     = "Exe";
    constexpr const char* field_dir     = "StartDir";
    constexpr const char* field_icon    = "icon";
    constexpr const char* field_options = "LaunchOptions";

    string
    quoted (const fs::path& p)
    {
      return "\"" + p.string () + "\"";
    }

    string
    directory (const fs::path& p)
    {
      string r (p.string ());

      if (!r.empty () && !r.ends_with (fs::path::preferred_separator))
        r += static_cast<char> (fs::path::preferred_separator);

      return r;
    }

    string
    command_line (const vector<string>& as)
    {
      string r;

      for (const string& a : as)
      {
        if (!r.empty ())
          r += ' ';

        if (a.find (' ') == string::npos)
          r += a;
        else
          r += "\"" + a + "\"";
      }

      return r;
    }

    // The launch options of the entry for this shortcut.
    //
    // Note that we pass --no-shortcuts: the refresh may close and reopen
    // Steam to update the entries, which, when Steam itself started us, is
    // taking down our own parent. Steam then goes on to relaunch us and the
    // whole thing repeats.
    //
    string
    launch_options (const shortcut_spec& c)
    {
      vector<string> as (c.arguments);
      as.push_back ("--no-shortcuts");

      return command_line (as);
    }

    optional<vector<char>>
    read_file (const fs::path& p)
    {
      ifstream i (p, ios::binary);

      if (!i)
        return nullopt;

      vector<char> d ((istreambuf_iterator<char> (i)),
                      istreambuf_iterator<char> ());

      if (i.bad ())
        throw runtime_error ("failed to read " + p.string ());

      return d;
    }

    void
    write_file (const fs::path& p, const vector<char>& d)
    {
      error_code ec;

      fs::create_directories (p.parent_path (), ec);

      if (ec)
        throw system_error (ec,
                            "failed to create " + p.parent_path ().string ());

      fs::path t (p);
      t += ".new";

      {
        ofstream o (t, ios::binary | ios::trunc);

        if (!o)
          throw runtime_error ("failed to create " + t.string ());

        o.write (d.data (), static_cast<streamsize> (d.size ()));
        o.flush ();

        if (!o)
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

    std::string
    next_key (const vdf_object& l)
    {
      long n (-1);

      for (const std::string& k : l.keys ())
      {
        if (k.empty () ||
            !all_of (k.begin (), k.end (), [] (unsigned char c)
            {
              return c >= '0' && c <= '9';
            }))
          continue;

        try
        {
          n = max (n, stol (k));
        }
        catch (const exception&)
        {
        }
      }

      return std::to_string (n + 1);
    }

    vdf_object*
    find_entry (vdf_object& l, const string& name)
    {
      for (const string& k : l.keys ())
      {
        vdf_object* e (l.object (k));

        if (e == nullptr)
          continue;

        const string* n (e->string (field_name));

        if (n != nullptr && *n == name)
          return e;
      }

      return nullptr;
    }

    uint32_t
    entry_app_id (const vdf_object& e)
    {
      const int32_t* v (e.number (field_app_id));
      return v != nullptr ? static_cast<uint32_t> (*v) : 0;
    }

    struct steam_program
    {
      string file;
      vector<string> args;
    };

    optional<steam_program>
    program (const fs::path& root);

    bool
    run (const steam_program& p, const vector<string>& extra = {})
    {
      namespace process = boost::process;
      namespace extend = boost::process::v1::extend;

      vector<string> as (p.args);
      as.insert (as.end (), extra.begin (), extra.end ());

      try
      {
        process::spawn (p.file,
                        process::args (as),
                        process::std_in  < process::null,
                        process::std_out > process::null,
                        process::std_err > process::null,
#ifdef _WIN32
                        extend::on_setup = [] (auto& e)
                        {
                          e.creation_flags |= DETACHED_PROCESS |
                                              CREATE_NEW_PROCESS_GROUP;
                        }
#else
                        extend::on_exec_setup = [] (auto&)
                        {
                          ::setsid ();
                        }
#endif
                        );
      }
      catch (const exception&)
      {
        return false;
      }

      return true;
    }

    bool
    await_exit ()
    {
      using namespace chrono;

      constexpr seconds limit (30);
      constexpr milliseconds poll (250);

      for (steady_clock::time_point d (steady_clock::now () + limit);
           steady_clock::now () < d;
           this_thread::sleep_for (poll))
      {
        if (!steam_shortcuts::running ())
          return true;
      }

      return !steam_shortcuts::running ();
    }

    struct entry_id
    {
      const shortcut_spec* spec;
      uint32_t app_id;
    };

    bool
    install_image (const fs::path& from, const fs::path& to, bool apply)
    {
      error_code ec;

      if (from.empty () || !fs::exists (from, ec))
        return false;

      if (fs::exists (to, ec) &&
          fs::file_size (from, ec) == fs::file_size (to, ec) &&
          !ec)
      {
        ifstream a (from, ios::binary), b (to, ios::binary);

        if (a && b)
        {
          string x ((istreambuf_iterator<char> (a)),
                    istreambuf_iterator<char> ());
          string y ((istreambuf_iterator<char> (b)),
                    istreambuf_iterator<char> ());

          if (x == y)
            return false;
        }
      }

      if (!apply)
        return true;

      fs::create_directories (to.parent_path (), ec);

      if (ec)
        throw system_error (ec,
                            "failed to create " + to.parent_path ().string ());

      fs::path t (to);
      t += ".new";

      fs::copy_file (from, t, fs::copy_options::overwrite_existing, ec);

      if (ec)
        throw system_error (ec, "failed to copy artwork to " + t.string ());

      fs::rename (t, to, ec);

      if (ec)
      {
        error_code ic;
        fs::remove (t, ic);

        throw system_error (ec, "failed to install " + to.string ());
      }

      return true;
    }

    bool
    install_artwork (const fs::path& grid, const entry_id& e, bool apply)
    {
      if (e.app_id == 0)
        return false;

      std::string id (std::to_string (e.app_id));

      const shortcut_spec& c (*e.spec);

      bool r (false);

      r |= install_image (c.library.wide, grid / (id + ".png"), apply);
      r |= install_image (c.library.cover, grid / (id + "p.png"), apply);
      r |= install_image (c.library.hero, grid / (id + "_hero.png"), apply);
      r |= install_image (c.library.logo, grid / (id + "_logo.png"), apply);

      return r;
    }

    bool
    remove_artwork (const fs::path& grid, uint32_t app_id, bool apply)
    {
      if (app_id == 0)
        return false;

      std::string id (std::to_string (app_id));

      bool r (false);

      for (const char* s : {".png", "p.png", "_hero.png", "_logo.png"})
      {
        fs::path p (grid / (id + s));

        error_code ec;

        if (!fs::exists (p, ec))
          continue;

        r = true;

        if (apply && !fs::remove (p, ec) && ec)
          throw system_error (ec, "failed to remove " + p.string ());
      }

      return r;
    }

    // Steam numbers the entries consecutively from 0. Restore that after
    // some were removed.
    //
    void
    renumber (vdf_object& l)
    {
      vector<vdf_object> es;

      for (const string& k : l.keys ())
      {
        if (const vdf_object* e = l.object (k))
        {
          es.push_back (*e);
          l.erase (k);
        }
      }

      for (size_t i (0); i != es.size (); ++i)
        l.set_object (std::to_string (i)) = std::move (es[i]);
    }

    // Remove the entries earlier launchers added under names the shortcuts
    // no longer carry, returning their app ids in stale. We recognize ours
    // by the launch options rather than the executable, in case the
    // installation has moved since.
    //
    bool
    retire (vdf_object& l,
            const vector<shortcut_spec>& cs,
            vector<uint32_t>& stale)
    {
      bool changed (false);

      for (const shortcut_spec& c : cs)
      {
        // Earlier launchers did not pass --no-shortcuts, so recognize
        // either.
        //
        string opt (launch_options (c));
        string old (command_line (c.arguments));

        for (const string& k : l.keys ())
        {
          const vdf_object* e (l.object (k));

          if (e == nullptr)
            continue;

          const string* n (e->string (field_name));
          const string* o (e->string (field_options));

          if (n == nullptr || o == nullptr || (*o != opt && *o != old) ||
              find (c.legacy_names.begin (), c.legacy_names.end (), *n) ==
                c.legacy_names.end ())
            continue;

          stale.push_back (entry_app_id (*e));
          l.erase (k);
          changed = true;
        }
      }

      if (changed)
        renumber (l);

      return changed;
    }

    bool
    reconcile (vdf_object& l,
               const vector<shortcut_spec>& cs,
               vector<entry_id>& ids,
               vector<uint32_t>& stale)
    {
      bool changed (retire (l, cs, stale));

      for (const shortcut_spec& c : cs)
      {
        string exe (quoted (c.target));
        string dir (directory (c.working_directory));
        string opt (launch_options (c));

        error_code ec;
        string icon (!c.image.empty () && fs::exists (c.image, ec)
                     ? c.image.string ()
                     : string ());

        vdf_object* e (find_entry (l, c.name));

        if (e == nullptr)
        {
          e = &l.set_object (next_key (l));

          e->set (field_app_id, steam_shortcuts::app_id (exe, c.name));
          ids.push_back ({&c, entry_app_id (*e)});
          e->set (field_name, c.name);
          e->set (field_exe, exe);
          e->set (field_dir, dir);
          e->set (field_icon, icon);
          e->set ("ShortcutPath", string ());
          e->set (field_options, opt);
          e->set ("IsHidden", 0);
          e->set ("AllowDesktopConfig", 1);
          e->set ("AllowOverlay", 1);
          e->set ("OpenVR", 0);
          e->set ("Devkit", 0);
          e->set ("DevkitGameID", string ());
          e->set ("DevkitOverrideAppID", 0);
          e->set ("LastPlayTime", 0);
          e->set ("FlatpakAppID", string ());
          e->set ("sortas", string ());
          e->set_object ("tags");

          changed = true;
          continue;
        }

        auto correct ([&e, &changed] (const char* f, const string& v)
        {
          const string* c (e->string (f));

          if (c != nullptr && *c == v)
            return;

          e->set (f, v);
          changed = true;
        });

        correct (field_exe, exe);
        correct (field_dir, dir);
        correct (field_options, opt);

        if (!icon.empty ())
          correct (field_icon, icon);

        ids.push_back ({&c, entry_app_id (*e)});
      }

      return changed;
    }
  }

  string_view
  to_string (steam_status s) noexcept
  {
    switch (s)
    {
      case steam_status::updated:       return "updated";
      case steam_status::restarted:     return "updated, steam restarted";
      case steam_status::up_to_date:    return "up to date";
      case steam_status::not_installed: return "not installed";
      case steam_status::no_users:      return "no signed-in accounts";
      case steam_status::running:       return "running and would not close";
      case steam_status::unreadable:    return "unreadable";
    }

    return "";
  }

  ostream&
  operator<< (ostream& o, steam_status s)
  {
    return o << to_string (s);
  }

  int32_t steam_shortcuts::
  app_id (const string& exe, const string& name)
  {
    string k (exe + name);

    mz_ulong c (mz_crc32 (MZ_CRC32_INIT,
                          reinterpret_cast<const unsigned char*> (k.data ()),
                          k.size ()));

    return static_cast<int32_t> (static_cast<uint32_t> (c) | 0x80000000u);
  }

  vector<fs::path> steam_shortcuts::
  lists (const fs::path& r)
  {
    vector<fs::path> ps;

    error_code ec;
    fs::directory_iterator i (r / "userdata", ec);

    if (ec)
      return ps;

    for (const fs::directory_entry& e : i)
    {
      if (!e.is_directory (ec))
        continue;

      string n (e.path ().filename ().string ());

      if (n.empty () || n == "0" ||
          !all_of (n.begin (), n.end (), [] (unsigned char c)
          {
            return c >= '0' && c <= '9';
          }))
        continue;

      ps.push_back (e.path () / "config" / "shortcuts.vdf");
    }

    return ps;
  }

  bool steam_shortcuts::
  shutdown (const fs::path& r)
  {
    optional<steam_program> p (program (r));

    if (!p || !run (*p, {"-shutdown"}))
      return false;

    return await_exit ();
  }

  bool steam_shortcuts::
  launch (const fs::path& r)
  {
    optional<steam_program> p (program (r));

    return p && run (*p);
  }

  steam_status steam_shortcuts::
  write (const vector<shortcut_spec>& cs)
  {
    optional<fs::path> r (root ());

    if (!r)
      return steam_status::not_installed;

    vector<fs::path> ps (lists (*r));

    if (ps.empty ())
      return steam_status::no_users;

    auto pass ([&cs] (const fs::path& p, bool apply, bool& unreadable)
    {
      optional<vector<char>> d (read_file (p));

      vdf_object doc;

      if (d)
      {
        try
        {
          doc = vdf_object::parse (*d);
        }
        catch (const exception&)
        {
          unreadable = true;
          return false;
        }

        if (doc.serialize () != *d)
        {
          unreadable = true;
          return false;
        }
      }

      vdf_object* l (doc.object (shortcuts_key));

      if (l == nullptr)
        l = &doc.set_object (shortcuts_key);

      vector<entry_id> ids;
      vector<uint32_t> stale;

      bool changed (reconcile (*l, cs, ids, stale));

      if (changed && apply)
        write_file (p, doc.serialize ());

      fs::path grid (p.parent_path () / "grid");

      for (uint32_t id : stale)
      {
        if (remove_artwork (grid, id, apply))
          changed = true;
      }

      for (const entry_id& e : ids)
      {
        if (install_artwork (grid, e, apply))
          changed = true;
      }

      return changed;
    });

    bool unreadable (false);
    bool work (false);

    for (const fs::path& p : ps)
      work = pass (p, false, unreadable) || work;

    if (!work)
      return unreadable ? steam_status::unreadable : steam_status::up_to_date;

    bool restart (false);

    if (running ())
    {
      if (!shutdown (*r))
        return steam_status::running;

      restart = true;
    }

    struct relaunch
    {
      const fs::path& root;
      bool armed;

      ~relaunch ()
      {
        if (armed)
          steam_shortcuts::launch (root);
      }
    } guard {*r, restart};

    unreadable = false;

    bool changed (false);

    for (const fs::path& p : ps)
      changed = pass (p, true, unreadable) || changed;

    if (changed)
      return restart ? steam_status::restarted : steam_status::updated;

    return unreadable ? steam_status::unreadable : steam_status::up_to_date;
  }

#ifdef _WIN32

  namespace
  {
    optional<string>
    registry_string (HKEY k, const char* sub, const char* name)
    {
      DWORD n (0);

      if (RegGetValueA (k, sub, name, RRF_RT_REG_SZ, nullptr, nullptr, &n) !=
          ERROR_SUCCESS)
        return nullopt;

      string v (n, '\0');

      if (RegGetValueA (k, sub, name, RRF_RT_REG_SZ, nullptr, v.data (), &n) !=
          ERROR_SUCCESS)
        return nullopt;

      v.resize (n == 0 ? 0 : n - 1);
      return v;
    }
  }

  optional<fs::path> steam_shortcuts::
  root ()
  {
    if (optional<string> p = registry_string (HKEY_CURRENT_USER,
                                              "Software\\Valve\\Steam",
                                              "SteamPath"))
    {
      if (!p->empty ())
        return fs::path (*p);
    }

    if (optional<string> p = registry_string (HKEY_LOCAL_MACHINE,
                                              "Software\\Valve\\Steam",
                                              "InstallPath"))
    {
      if (!p->empty ())
        return fs::path (*p);
    }

    return nullopt;
  }

  namespace
  {
    optional<steam_program>
    program (const fs::path& r)
    {
      fs::path e (r / "steam.exe");

      error_code ec;

      if (!fs::exists (e, ec))
        return nullopt;

      return steam_program {e.string (), {}};
    }
  }

  bool steam_shortcuts::
  running ()
  {
    DWORD v (0);
    DWORD n (sizeof (v));

    if (RegGetValueA (HKEY_CURRENT_USER,
                      "Software\\Valve\\Steam\\ActiveProcess",
                      "pid",
                      RRF_RT_REG_DWORD,
                      nullptr,
                      &v,
                      &n) != ERROR_SUCCESS)
      return false;

    return v != 0;
  }

#else

  namespace
  {
    vector<fs::path>
    candidate_roots ()
    {
      const char* h (getenv ("HOME"));

      if (h == nullptr || *h == '\0')
        return {};

      fs::path d (h);

      return {
        d / ".steam" / "steam",
        d / ".steam" / "root",

        d / ".local" / "share" / "Steam",

        d / ".var" / "app" / "com.valvesoftware.Steam" / ".local" /
          "share" / "Steam"
      };
    }
  }

  optional<fs::path> steam_shortcuts::
  root ()
  {
    error_code ec;

    for (const fs::path& p : candidate_roots ())
    {
      if (!fs::is_directory (p / "userdata", ec))
        continue;

      fs::path c (fs::canonical (p, ec));

      return ec ? p : c;
    }

    return nullopt;
  }

  namespace
  {
    optional<steam_program>
    program (const fs::path& r)
    {
      if (r.string ().find ("/com.valvesoftware.Steam/") != string::npos)
        return steam_program {"flatpak", {"run", "com.valvesoftware.Steam"}};

      string w (boost::process::search_path ("steam").string ());

      if (!w.empty ())
        return steam_program {std::move (w), {}};

      fs::path e (r / "steam.sh");

      error_code ec;

      if (fs::exists (e, ec))
        return steam_program {e.string (), {}};

      return nullopt;
    }
  }

  bool steam_shortcuts::
  running ()
  {
    const char* h (getenv ("HOME"));

    if (h == nullptr || *h == '\0')
      return false;

    for (const char* n : {".steam/steam.pid",
                          ".var/app/com.valvesoftware.Steam/.steam/steam.pid"})
    {
      ifstream i (fs::path (h) / n);

      if (!i)
        continue;

      long v (0);

      if (!(i >> v) || v <= 0)
        continue;

      if (kill (static_cast<pid_t> (v), 0) == 0 || errno == EPERM)
        return true;
    }

    return false;
  }

#endif
}
