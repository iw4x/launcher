#include <launcher/steam/steam-proton.hxx>

#include <boost/process.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/steady_timer.hpp>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <regex>

using namespace std;
using namespace std::chrono_literals;

namespace launcher
{
  namespace bp = boost::process;

  // Helpers
  //

  string
  to_string (proton_status s)
  {
    switch (s)
    {
      case proton_status::not_found:    return "not-found";
      case proton_status::found:        return "found";
      case proton_status::incompatible: return "incompatible";
    }

    return "unknown";
  }

  string
  to_string (ghost_result r)
  {
    switch (r)
    {
      case ghost_result::steam_running:     return "steam-running";
      case ghost_result::steam_not_running: return "steam-not-running";
      case ghost_result::error:             return "error";
    }

    return "unknown";
  }

  bool
  is_steam_deck ()
  {
    ifstream f ("/etc/os-release");

    if (!f.is_open ())
      return (false);

    for (string l; getline (f, l); )
    {
      // Look for OS ID. SteamOS identifies itself clearly here.
      //
      if (l.find ("ID=") == 0)
        return (l.find ("steamos") != string::npos);
    }

    return false;
  }

  static bool
  _pgrep_is_steam_running ()
  {
    bp::ipstream is;
    bp::child c ("pgrep -x steam", bp::std_out > is);

    c.wait ();
    return c.exit_code () == 0;
  }

  // proton_environment
  //

  map<string, string> proton_environment::
  build_env_map () const
  {
    map<string, string> env;

    // Tell generated logs to use a predictable filename (steam-10190.log).
    //
    env["SteamAppId"] = "10190";
    env["SteamGameId"] = "10190";

    // These are the magic environment variables Proton needs to know where
    // to put its fake Windows C: drive and where to look for Steam libraries.
    //
    env["STEAM_COMPAT_DATA_PATH"] = compatdata_path.string ();
    env["STEAM_COMPAT_CLIENT_INSTALL_PATH"] = client_install_path.string ();

    // If we are on Deck, we need need LAA or the 32-bit address space may gets
    // exhausted.
    //
    if (is_steam_deck ())
      env["PROTON_FORCE_LARGE_ADDRESS_AWARE"] = "1";

    if (enable_logging)
    {
      env["PROTON_LOG"] = "1";
      env["PROTON_LOG_DIR"] = log_dir.string ();
    }

    return env;
  }

  // proton_manager
  //

  proton_manager::
  proton_manager (asio::io_context& ioc)
    : ioc_ (ioc)
  {
  }

  optional<string> proton_manager::
  parse_version (const string& name)
  {
    // Valve isn't exactly consistent with naming. We see things like:
    // - "Proton 9.0"
    // - "Proton 8.0-5"
    // - "Proton - Experimental"
    //
    // We try to grab the first numeric version string we find.
    //
    static const regex re (R"(Proton\s+(\d+\.\d+(?:-\d+)?))");
    smatch m;

    if (regex_search (name, m, re) && m.size () > 1)
      return m[1].str ();

    // Special case for Experimental, which usually doesn't have a number
    // but is generally "newer" than stable.
    //
    if (name.find ("Experimental") != string::npos)
      return string ("experimental");

    return nullopt;
  }

  bool proton_manager::
  version_compare (const proton_version& a, const proton_version& b)
  {
    // We treat Experimental as "newer" than everything else because it
    // usually has the latest fixes we need.
    //
    if (a.experimental && !b.experimental) return true;
    if (!a.experimental && b.experimental) return false;

    // Otherwise, simple lexicographical sort is usually good enough for
    // "8.0" vs "7.0".
    //
    return a.version > b.version;
  }

  asio::awaitable<vector<proton_version>> proton_manager::
  detect_proton_versions (const fs::path& steam_path)
  {
    // We are going to scan the `steamapps/common` directory. It's a bit
    // of a brute-force approach, but it's the most reliable way to find
    // what's actually installed on disk.
    //
    auto executor (co_await asio::this_coro::executor);

    vector<proton_version> r;

    fs::path common (steam_path / "steamapps" / "common");

    if (!fs::exists (common))
      co_return r;

    try
    {
      for (const auto& entry : fs::directory_iterator (common))
      {
        if (!entry.is_directory ())
          continue;

        string name (entry.path ().filename ().string ());

        // Filter for directories starting with "Proton".
        //
        if (name.find ("Proton") != 0)
          continue;

        // Verify it's actually a Proton install by looking for the script.
        //
        fs::path bin (entry.path () / "proton");
        if (!fs::exists (bin))
          continue;

        proton_version pv;
        pv.path = entry.path ();
        pv.name = name;
        pv.experimental = (name.find ("Experimental") != string::npos);

        auto v (parse_version (name));
        pv.version = v ? *v : name; // Fallback to full name if parsing fails.

        r.push_back (move (pv));
      }
    }
    catch (const fs::filesystem_error& e)
    {
      // If we can't read the directory (permissions?), just warn and return
      // whatever we found so far.
      //
      cerr << "warning: failed to scan for Proton: " << e.what () << "\n";
    }

    // Sort newest/best first.
    //
    sort (r.begin (), r.end (), version_compare);

    co_return r;
  }

  asio::awaitable<optional<proton_version>> proton_manager::
  find_best_proton (const fs::path& steam_path)
  {
    auto vs (co_await detect_proton_versions (steam_path));

    if (vs.empty ())
      co_return nullopt;

    // Since we sorted them, the first one is our best bet.
    //
    co_return vs.front ();
  }

  proton_environment proton_manager::
  build_environment (const fs::path& steam_path,
                     const proton_version& proton,
                     uint32_t appid,
                     bool enable_logging)
  {
    proton_environment env;

    env.steam_root = steam_path;

    // The compatdata directory is where the Wine prefix lives. We map it
    // by AppID so it doesn't conflict with other games.
    //
    env.compatdata_path = steam_path / "steamapps" / "compatdata" /
                          std::to_string (appid);
    env.client_install_path = steam_path;
    env.proton_bin = proton.path / "proton";
    env.appid = appid;
    env.enable_logging = enable_logging;

    if (enable_logging)
      env.log_dir = fs::current_path () / "proton_logs";

    return env;
  }

  asio::awaitable<void> proton_manager::
  create_steam_appid (const fs::path& dir, uint32_t appid)
  {
    // Proton/Steam API needs to see steam_appid.txt next to the executable
    // to know what game context to initialize.
    //
    auto executor (co_await asio::this_coro::executor);

    fs::path f (dir / "steam_appid.txt");

    try
    {
      ofstream os (f);
      if (!os)
        throw runtime_error ("failed to create steam_appid.txt");

      os << appid;
    }
    catch (const exception& e)
    {
      throw runtime_error (string ("failed to create steam_appid.txt: ") +
                           e.what ());
    }

    co_return;
  }

  asio::awaitable<ghost_result> proton_manager::
  run_ghost_process (const proton_environment& e, const fs::path& h)
  {
    auto ex (co_await asio::this_coro::executor);
    asio::steady_timer t (ex);

    // Proton gets grumpy if it can't find the prefix root when bootstrapping
    // its environment.
    //
    if (!fs::exists (e.compatdata_path))
    {
      error_code ec;
      fs::create_directories (e.compatdata_path, ec);

      if (ec)
      {
        cerr << "error: failed to create compatdata directory "
            << e.compatdata_path << ": " << ec.message () << endl;

        co_return ghost_result::error;
      }
    }

    // Steam Deck is a special kind of pain. It's a constrained environment
    // where our ghost usually fails because of missing .NET runtimes (an
    // implicit dependency of steam_api itself).
    //
    // For now we just grep for the process. It's brittle and we lose the
    // handshake, but it's what works.
    //
    // @@ TODO: This really belongs in setup_for_launch() in steam-proton.cxx.
    //
    if (is_steam_deck ())
    {
      for (int i (0); i < 3; ++i)
      {
        if (_pgrep_is_steam_running ())
          co_return ghost_result::steam_running;

        // Steam isn't running, so try to kick it. There is a theoretical
        // race here if Steam starts externally between our check and the
        // spawn, but the steam binary handles its own locking, so the
        // second instance will just bail out safely.
        //
        try
        {
          bp::child s ("steam");
          s.detach ();
        }
        catch (...) {}

        // Give it a moment to spin up.
        //
        t.expires_after (5s);
        co_await t.async_wait (asio::use_awaitable);
      }

      cerr << "error: failed to start steam within the timeout period" << endl;
      cerr << "falling back to wine is not supported on steamdeck" << endl;

      // @@ TODO: We should probably propagate this properly instead of
      // just nuking the process.
      //
      exit (1);
    }

    // On standard desktops we can do a proper probe.
    //
    try
    {
      bp::environment pe (boost::this_process::environment ());
      auto m (e.build_env_map ());

      for (const auto& [k, v] : m)
        pe[k] = v;

      bp::ipstream o;
      bp::ipstream er;
      bp::child g (
        e.proton_bin.string (),
        bp::args ({string ("run"), h.string (), string ("check")}),
        pe,
        bp::std_out > o,
        bp::std_err > er
      );

      g.wait ();

      string r;
      getline (o, r);

      // Keep the error output around in the logs so we can actually
      // debug it when the probe fails.
      //
      string l;
      while (getline (er, l))
        cerr << "ghost process: " << l << "\n";

      if (g.exit_code () == 0 && r == "running")
        co_return ghost_result::steam_running;
      else
        co_return ghost_result::steam_not_running;
    }
    catch (const exception& ex)
    {
      cerr << "error: failed to run ghost process: " << ex.what () << "\n";
      co_return ghost_result::error;
    }
  }

  asio::awaitable<bool> proton_manager::
  launch_through_proton (const proton_environment& e,
                         const fs::path& x,
                         const vector<string>& as)
  {
    if (!fs::exists (e.proton_bin))
    {
      cerr << "error: proton binary not found: " << e.proton_bin << "\n";
      co_return false;
    }

    try
    {
      // Prepare the environment variables.
      //
      bp::environment pe (boost::this_process::environment ());
      auto m (e.build_env_map ());

      for (const auto& [k, v] : m)
        pe[k] = v;

      string b;           // Binary to run.
      vector<string> cs;  // Command strings.

      if (is_steam_deck ())
      {
        // On Deck we have to wrap everything in the sniper runtime container.
        // It's a nesting doll situation:
        //
        // 1. reaper:  Keeps track of the process tree.
        // 2. wrapper: Sets up LD_LIBRARY_PATH and bootstrap.
        // 3. sniper:  The actual container switch (usually _v2-entry-point).
        // 4. proton:  The WINE runner.
        //
        auto r (e.steam_root / "ubuntu12_32" / "reaper");
        auto w (e.steam_root / "ubuntu12_32" / "steam-launch-wrapper");
        auto s (e.steam_root / "steamapps"   / "common" /
                "SteamLinuxRuntime_sniper"   / "_v2-entry-point");

        b = r.string ();

        cs.push_back ("SteamLaunch");
        cs.push_back ("AppId=" + std::to_string (e.appid));
        cs.push_back ("--");
        cs.push_back (w.string ());
        cs.push_back ("--");
        cs.push_back (s.string ());
        cs.push_back ("--verb=waitforexitandrun");
        cs.push_back ("--");
        cs.push_back (e.proton_bin.string ());
        cs.push_back ("waitforexitandrun");
        cs.push_back (x.string ());
      }
      else
      {
        // Standard Proton run.
        //
        b = e.proton_bin.string ();
        cs.push_back ("run");
        cs.push_back (x.string ());
      }

      // Append user arguments.
      //
      for (const auto& a : as)
        cs.push_back (a);

      // Launch and detach. We don't want the launcher to hang around blocking
      // the terminal while the game is running, nor do we want the game to
      // die if the launcher is closed.
      //
      bp::child c (
        b,
        bp::args (cs),
        pe,
        bp::std_out > bp::null,
        bp::std_err > bp::null,
        bp::start_dir (x.parent_path ().string ()));

      c.detach ();

      co_return true;
    }
    catch (const exception& e)
    {
      cerr << "error: failed to launch through Proton: " << e.what () << "\n";
      co_return false;
    }
  }
}
