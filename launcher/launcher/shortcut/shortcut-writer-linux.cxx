#include <launcher/shortcut/shortcut-writer.hxx>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

using namespace std;

namespace launcher
{
  namespace
  {
    string
    escape_value (const string& s)
    {
      string r;
      r.reserve (s.size ());

      for (char c : s)
      {
        switch (c)
        {
          case '\\': r += "\\\\"; break;
          case '\n': r += "\\n";  break;
          case '\r': r += "\\r";  break;
          case '\t': r += "\\t";  break;
          default:   r += c;      break;
        }
      }

      return r;
    }

    string
    escape_argument (const string& s)
    {
      string r ("\"");

      for (char c : s)
      {
        if (c == '"' || c == '`' || c == '$' || c == '\\')
          r += '\\';

        r += c;
      }

      r += '"';
      return r;
    }

    optional<fs::path>
    data_home ()
    {
      if (const char* d = getenv ("XDG_DATA_HOME"))
      {
        fs::path p (d);

        if (p.is_absolute ())
          return p;
      }

      if (const char* h = getenv ("HOME"))
      {
        fs::path p (h);

        if (p.is_absolute ())
          return p / ".local" / "share";
      }

      return nullopt;
    }

    bool
    write_if_changed (const fs::path& p, const string& s)
    {
      {
        ifstream i (p, ios::binary);

        if (i)
        {
          string c ((istreambuf_iterator<char> (i)),
                    istreambuf_iterator<char> ());

          if (c == s)
            return false;
        }
      }

      error_code ec;

      fs::create_directories (p.parent_path (), ec);

      if (ec)
        throw system_error (ec,
                            "failed to create shortcut directory: " +
                              p.parent_path ().string ());

      fs::path t (p);
      t += ".new";

      {
        ofstream o (t, ios::binary | ios::trunc);

        if (!o)
          throw runtime_error ("failed to create shortcut file: " +
                               t.string ());

        o << s;
        o.flush ();

        if (!o)
          throw runtime_error ("failed to write shortcut file: " +
                               t.string ());
      }

      fs::rename (t, p, ec);

      if (ec)
      {
        error_code ic;
        fs::remove (t, ic);

        throw system_error (ec,
                            "failed to install shortcut file: " + p.string ());
      }

      return true;
    }
  }

  bool shortcut_writer::
  supported (shortcut_scope s) noexcept
  {
    return s == shortcut_scope::menu;
  }

  optional<fs::path> shortcut_writer::
  directory (shortcut_scope s)
  {
    if (!supported (s))
      return nullopt;

    if (optional<fs::path> d = data_home ())
      return *d / "applications";

    return nullopt;
  }

  optional<fs::path> shortcut_writer::
  location (const shortcut_spec& c, shortcut_scope s)
  {
    if (optional<fs::path> d = directory (s))
      return *d / (c.id + ".desktop");

    return nullopt;
  }

  bool shortcut_writer::
  write (const shortcut_spec& c, shortcut_scope s)
  {
    optional<fs::path> p (location (c, s));

    if (!p)
      return false;

    string exec (escape_argument (c.target.string ()));

    for (const string& a : c.arguments)
    {
      exec += ' ';
      exec += escape_argument (a);
    }

    ostringstream o;

    o << "[Desktop Entry]\n"
      << "Type=Application\n"
      << "Version=1.5\n"
      << "Name=" << escape_value (c.name) << '\n';

    if (!c.comment.empty ())
      o << "Comment=" << escape_value (c.comment) << '\n';

    o << "Exec=" << escape_value (exec) << '\n';

    if (!c.working_directory.empty ())
      o << "Path=" << escape_value (c.working_directory.string ()) << '\n';

    error_code ec;

    if (!c.icon.empty () && fs::exists (c.icon, ec))
      o << "Icon=" << escape_value (c.icon.string ()) << '\n';

    o << "Terminal=true\n"
      << "Categories=Game;ActionGame;\n";

    return write_if_changed (*p, o.str ());
  }
}
