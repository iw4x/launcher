#include <launcher/launcher-steam-proton.hxx>

#include <boost/process.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/steady_timer.hpp>

#include <iostream>

using namespace std;
using namespace std::chrono_literals;

namespace launcher
{
  namespace bp = boost::process;

  proton_coordinator::
  proton_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      manager_ (make_unique<manager_type> (ioc)),
      verbose_ (false),
      enable_logging_ (false)
  {
  }

  void proton_coordinator::
  set_verbose (bool v)
  {
    verbose_ = v;
  }

  bool proton_coordinator::
  verbose () const
  {
    return verbose_;
  }

  void proton_coordinator::
  set_enable_logging (bool v)
  {
    enable_logging_ = v;
  }

  bool proton_coordinator::
  enable_logging () const
  {
    return enable_logging_;
  }

  // Version detection & selection.
  //

  asio::awaitable<vector<proton_coordinator::version_type>> proton_coordinator::
  detect_versions (const fs::path& steam_path)
  {
    co_return co_await manager_->detect_proton_versions (steam_path);
  }

  asio::awaitable<optional<proton_coordinator::version_type>> proton_coordinator::
  find_best_version (const fs::path& steam_path)
  {
    co_return co_await manager_->find_best_proton (steam_path);
  }

  proton_coordinator::environment_type proton_coordinator::
  prepare_environment (const fs::path& steam_path,
                       const version_type& proton,
                       uint32_t appid)
  {
    return manager_->build_environment (steam_path,
                                        proton,
                                        appid,
                                        enable_logging_);
  }

  // Launch orchestration.
  //

  asio::awaitable<bool> proton_coordinator::
  setup_for_launch (const environment_type& env,
                    const fs::path& game_dir,
                    const fs::path& launcher_dir)
  {
    // We need to set the stage for the Steam API to work correctly inside the
    // Proton container.
    //
    if (verbose_)
      cout << "Setting up for launch..." << "\n";

    // The game itself needs steam_appid.txt in its CWD to initialize the API
    // without Steam's launcher forcing a restart.
    //
    try
    {
      co_await manager_->create_steam_appid (game_dir, env.appid);

      if (verbose_)
        cout << "Created steam_appid.txt in game directory." << "\n";
    }
    catch (const exception& e)
    {
      cerr << "error: " << e.what () << "\n";
      co_return false;
    }

    // Our 'steam.exe' helper tool (which runs inside Proton) also needs to
    // initialize the Steam API to probe status. So it needs an ID file too.
    //
    try
    {
      co_await manager_->create_steam_appid (launcher_dir, env.appid);

      if (verbose_)
        cout << "Created steam_appid.txt in launcher directory." << "\n";
    }
    catch (const exception& e)
    {
      // This is a warning, not an error, because maybe the game can still
      // run even if our helper fails to check status.
      //
      cerr
        << "warning: failed to create steam_appid.txt in launcher directory: "
        << e.what () << "\n";
    }

    // Now we run the helper inside the container to see if Steam is actually
    // reachable from that environment.
    //
    fs::path helper (launcher_dir / "steam.exe");

    if (!fs::exists (helper))
    {
      cerr << "warning: steam.exe helper not found at " << helper << "\n"
           << "Assuming Steam is not running." << "\n";
      co_return false;
    }

    if (verbose_)
      cout << "Running ghost process to check Steam status..." << "\n";

    auto r (co_await manager_->run_ghost_process (env, helper));

    if (r == ghost_result::steam_running)
    {
      if (verbose_)
        cout << "Steam is running and initialized." << "\n";

      co_return true;
    }
    else if (r == ghost_result::steam_not_running)
    {
      if (verbose_)
        cout << "Steam is not running." << "\n";

      co_return false;
    }
    else
    {
      cerr << "error: failed to check Steam status" << "\n";
      co_return false;
    }
  }

  asio::awaitable<bool> proton_coordinator::
  launch (const environment_type& env,
          const fs::path& exe,
          const vector<string>& args)
  {
    if (verbose_)
    {
      cout << "Launching through Proton..." << "\n"
           << "  Executable: " << exe << "\n"
           << "  Proton:     " << env.proton_bin << "\n";

      if (!args.empty ())
      {
        cout << "  Arguments: ";
        for (const auto& a : args) cout << " " << a;
        cout << "\n";
      }
    }

    co_return co_await manager_->launch_through_proton (env, exe, args);
  }

  asio::awaitable<bool> proton_coordinator::
  complete_launch (const fs::path& steam_path,
                   const fs::path& exe,
                   uint32_t appid,
                   const vector<string>& args)
  {
    // The main event. We orchestrate the entire startup sequence here.
    //

    if (verbose_)
      cout << "Detecting Proton versions..." << "\n";

    auto proton (co_await find_best_version (steam_path));

    if (!proton)
    {
      cerr << "error: no suitable Proton version found" << "\n";
      co_return false;
    }

    if (verbose_)
      cout << "Using Proton: " << proton->name << "\n";

    auto env (prepare_environment (steam_path, *proton, appid));

    fs::path game_dir (exe.parent_path ());
    fs::path launcher_dir (fs::current_path ());

    bool running (co_await setup_for_launch (env, game_dir, launcher_dir));

    if (!running)
    {
      if (verbose_)
        cout << "Steam is not running. Attempting to start Steam..." << "\n";

      if (!co_await start_steam ())
      {
        cerr << "warning: failed to start Steam" << "\n"
             << "Launching anyway, Steam features may not work." << "\n";
      }
      else
      {
        // Steam takes a moment to spin up the IPC pipes.
        //
        if (verbose_)
          cout << "Waiting for Steam to initialize..." << "\n";

        asio::steady_timer t (ioc_, 5s);
        co_await t.async_wait (asio::use_awaitable);

        // Double check.
        //
        running = co_await setup_for_launch (env, game_dir, launcher_dir);

        if (running && verbose_)
          cout << "Steam is now running." << "\n";
      }
    }

    co_return co_await launch (env, exe, args);
  }

  asio::awaitable<bool> proton_coordinator::
  is_steam_running (const environment_type& env,
                    const fs::path& helper)
  {
    auto r (co_await manager_->run_ghost_process (env, helper));
    co_return (r == ghost_result::steam_running);
  }

  asio::awaitable<bool> proton_coordinator::
  start_steam ()
  {
    try
    {
      // Just kick off the process and hope it finds the installed client.
      //
      bp::child c (
        bp::search_path ("steam"),
        bp::std_out > bp::null,
        bp::std_err > bp::null
      );

      c.detach ();

      if (verbose_)
        cout << "Steam started." << "\n";

      // Give it a brief moment to create the process before we return,
      // though the pipe initialization happens later.
      //
      asio::steady_timer t (ioc_, 3s);
      co_await t.async_wait (asio::use_awaitable);

      co_return true;
    }
    catch (const exception& e)
    {
      cerr << "error: failed to start Steam: " << e.what () << "\n";
      co_return false;
    }
  }

  proton_coordinator::manager_type& proton_coordinator::
  manager ()
  {
    return *manager_;
  }

  const proton_coordinator::manager_type& proton_coordinator::
  manager () const
  {
    return *manager_;
  }
}
