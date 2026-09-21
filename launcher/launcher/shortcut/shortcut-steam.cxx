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

    bool
    reconcile (vdf_object& l, const vector<shortcut_spec>& cs)
    {
      bool changed (false);

      for (const shortcut_spec& c : cs)
      {
        string exe (quoted (c.target));
        string dir (directory (c.working_directory));
        string opt (command_line (c.arguments));

        error_code ec;
        string icon (!c.icon.empty () && fs::exists (c.icon, ec)
                     ? c.icon.string ()
                     : string ());

        vdf_object* e (find_entry (l, c.name));

        if (e == nullptr)
        {
          e = &l.set_object (next_key (l));

          e->set (field_app_id, steam_shortcuts::app_id (exe, c.name));
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

      bool changed (reconcile (*l, cs));

      if (changed && apply)
        write_file (p, doc.serialize ());

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
