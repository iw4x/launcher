#include <launcher/launcher-steam-proton.hxx>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/process.hpp>

#include <iostream>

using namespace std;
using namespace std::chrono_literals;

namespace launcher
{
  namespace bp = boost::process;

  proton_coordinator::
  proton_coordinator (asio::io_context& c)
    : ioc_ (c),
      manager_ (make_unique<manager_type> (c)),
      verbose_ (false),
      logging_ (false)
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
    logging_ = v;
  }

  bool proton_coordinator::
  enable_logging () const
  {
    return logging_;
  }

  // Version detection & selection.
  //

  asio::awaitable<vector<proton_coordinator::version_type>> proton_coordinator::
  detect_versions (const fs::path& sp)
  {
    // Delegate the filesystem scanning to the manager logic. We just want
    // the list.
    //
    co_return co_await manager_->detect_proton_versions (sp);
  }

  asio::awaitable<optional<proton_coordinator::version_type>> proton_coordinator::
  find_best_version (const fs::path& sp)
  {
    co_return co_await manager_->find_best_proton (sp);
  }

  proton_coordinator::environment_type proton_coordinator::
  prepare_environment (const fs::path& sp,
                       const version_type& p,
                       uint32_t id)
  {
    // Assemble the environment variables (LD_LIBRARY_PATH, etc.) required for
    // the container. We inject the logging flag here if requested.
    //
    return manager_->build_environment (sp, p, id, logging_);
  }

  // Launch orchestration.
  //

  asio::awaitable<bool> proton_coordinator::
  setup_for_launch (const environment_type& e,
                    const fs::path& gd,
                    const fs::path& ld)
  {
    // We need to set the stage for the Steam API to work correctly inside the
    // Proton container. If the environment isn't primed, the game might crash
    // or, worse, loop back to the official Steam launcher.
    //

    if (verbose_)
      cout << "setting up for launch..." << "\n";

    // First, plant steam_appid.txt in the game's CWD. The Steam API DLLs look
    // for this to identify the application context. Without it,
    // SteamAPI_Init() fails.
    //
    try
    {
      co_await manager_->create_steam_appid (gd, e.appid);

      if (verbose_)
        cout << "created steam_appid.txt in game directory." << "\n";
    }
    catch (const exception& ex)
    {
      cerr << "error: " << ex.what () << "\n";
      co_return false;
    }

    // Our helper tool (steam.exe) runs inside Proton to probe the status.
    // Since it also links against the Steam API, it needs the ID file in its
    // own CWD (the launcher directory).
    //
    try
    {
      co_await manager_->create_steam_appid (ld, e.appid);

      if (verbose_)
        cout << "created steam_appid.txt in launcher directory." << "\n";
    }
    catch (const exception& ex)
    {
      // Not fatal. The game might still run even if our probe fails.
      //
      cerr << "warning: failed to create steam_appid.txt in launcher directory: "
           << ex.what () << "\n";
    }

    // Verify the "ghost" process. We run a dummy executable inside the
    // container. This proves that 1) Proton works, and 2) The container can
    // talk to the Steam IPC pipes.
    //
    fs::path h (gd / "steam.exe");

    if (!fs::exists (h))
    {
      cerr << "warning: steam.exe helper not found at " << h << "\n"
           << "assuming Steam is not running." << "\n";
      co_return false;
    }

    if (verbose_)
      cout << "running ghost process to check steam status..." << "\n";

    auto r (co_await manager_->run_ghost_process (e, h));

    if (r == ghost_result::steam_running)
    {
      if (verbose_)
        cout << "steam is running and initialized." << "\n";

      co_return true;
    }
    else if (r == ghost_result::steam_not_running)
    {
      if (verbose_)
        cout << "steam is not running." << "\n";

      co_return false;
    }
    else
    {
      cerr << "error: failed to check steam status" << "\n";
      co_return false;
    }
  }

  asio::awaitable<bool> proton_coordinator::
  launch (const environment_type& e,
          const fs::path& x,
          const vector<string>& as)
  {
    if (verbose_)
    {
      cout << "launching through proton..." << "\n"
           << "  executable: " << x << "\n"
           << "  proton:     " << e.proton_bin << "\n";

      if (!as.empty ())
      {
        cout << "  arguments: ";
        for (const auto& a : as) cout << " " << a;
        cout << "\n";
      }
    }

    co_return co_await manager_->launch_through_proton (e, x, as);
  }

  asio::awaitable<bool> proton_coordinator::
  complete_launch (const fs::path& sp,
                   const fs::path& x,
                   uint32_t id,
                   const vector<string>& as)
  {
    // Orchestrate the full startup sequence. We have to be careful about the
    // order: find the runtime, prep the sandbox, ensure Steam is alive, and
    // finally exec.
    //

    if (verbose_)
      cout << "detecting proton versions..." << "\n";

    auto p (co_await find_best_version (sp));

    if (!p)
    {
      cerr << "error: no suitable proton version found" << "\n";
      co_return false;
    }

    if (verbose_)
      cout << "using proton: " << p->name << "\n";

    auto e (prepare_environment (sp, *p, id));

    fs::path gd (x.parent_path ());
    fs::path ld (fs::current_path ());

    bool r (co_await setup_for_launch (e, gd, ld));

    if (!r)
    {
      // The IPC check failed or Steam isn't there. Try to wake it up.
      //
      if (verbose_)
        cout << "steam is not running. attempting to start Steam..." << "\n";

      if (!co_await start_steam ())
      {
        cerr << "warning: failed to start steam" << "\n"
             << "launching anyway, steam features may not work." << "\n";
      }
      else
      {
        // Steam started, but the pipes take a moment to initialize.
        // Sleep briefly before retrying the setup.
        //
        if (verbose_)
          cout << "waiting for steam to initialize..." << "\n";

        asio::steady_timer t (ioc_, 5s);
        co_await t.async_wait (asio::use_awaitable);

        r = (co_await setup_for_launch (e, gd, ld));

        if (r && verbose_)
          cout << "steam is now running." << "\n";
      }
    }

    co_return co_await launch (e, x, as);
  }

  asio::awaitable<bool> proton_coordinator::
  is_steam_running (const environment_type& e,
                    const fs::path& h)
  {
    auto r (co_await manager_->run_ghost_process (e, h));
    co_return r == ghost_result::steam_running;
  }

  asio::awaitable<bool> proton_coordinator::
  start_steam ()
  {
    try
    {
      // We don't track the Steam process. Just kick it off detatched. Rely on
      // the system PATH to find the binary.
      //
      bp::child c (
        bp::search_path ("steam"),
        bp::std_out > bp::null,
        bp::std_err > bp::null);

      c.detach ();

      if (verbose_)
        cout << "steam started." << "\n";

      // Yield briefly to give the process time to register the PID, though
      // pipe creation usually takes longer (handled by caller).
      //
      asio::steady_timer t (ioc_, 3s);
      co_await t.async_wait (asio::use_awaitable);

      co_return true;
    }
    catch (const exception& ex)
    {
      cerr << "error: failed to start steam: " << ex.what () << "\n";
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
