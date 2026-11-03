// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/launcher-steam-proton.hxx>

#include <cerrno>
#include <chrono>
#include <cstdlib>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <launcher/process.hxx>
#include <launcher/launcher-log.hxx>

using namespace std;
using namespace std::chrono_literals;

namespace launcher
{
  namespace
  {
    // The Companion launcher that the rawfiles release ships in the game
    // directory. It starts the native Steam client with the module that
    // makes Steam friends see IW4x running under Proton as the game being
    // played.
    //
    fs::path
    companion_program (const fs::path& gd)
    {
      return gd / "linux" / "companion";
    }

    bool
    companion_installed (const fs::path& gd)
    {
      error_code ec;
      return fs::is_regular_file (companion_program (gd), ec);
    }

    // Return true if the Companion module is active in the running Steam
    // client. The module holds an exclusive lock on its status file for as
    // long as it runs, so a shared lock is only refused while it does. Anyone
    // can create files in /dev/shm, so we only consider a regular file that
    // we own.
    //
    bool
    companion_active ()
    {
      string p ("/dev/shm/companion." + std::to_string (getuid ()));

      int f (open (p.c_str (), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));

      if (f == -1)
        return false;

      struct stat s;
      bool r (fstat (f, &s) == 0                 &&
              S_ISREG (s.st_mode)                &&
              s.st_uid == geteuid ()             &&
              flock (f, LOCK_SH | LOCK_NB) != 0  &&
              errno == EWOULDBLOCK);

      close (f);
      return r;
    }
  }

  proton_coordinator::
  proton_coordinator (asio::io_context& c, bool fsr)
    : ioc_ (c),
      manager_ (c, fsr),
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

  asio::awaitable<vector<proton_coordinator::version_type>> proton_coordinator::
  detect_versions (const fs::path& sp)
  {
    co_return co_await manager_.detect_proton_versions (sp);
  }

  asio::awaitable<optional<proton_coordinator::version_type>> proton_coordinator::
  find_best_version (const fs::path& sp)
  {
    co_return co_await manager_.find_best_proton (sp);
  }

  proton_coordinator::environment_type proton_coordinator::
  prepare_environment (const fs::path& sp,
                       const version_type& p,
                       uint32_t id)
  {
    return manager_.build_environment (sp, p, id, logging_);
  }

  asio::awaitable<bool> proton_coordinator::
  setup_for_launch (const environment_type& e,
                    const fs::path& gd,
                    const fs::path& ld)
  {
    if (verbose_)
      launcher::log::info (categories::steam{}, "setting up for launch");

    // The game runs in its directory and without steam_appid.txt there its
    // SteamAPI_Init() fails.
    //
    try
    {
      co_await manager_.create_steam_appid (gd, e.appid);

      if (verbose_)
        launcher::log::info (categories::steam{}, "created steam_appid.txt in game directory");
    }
    catch (const exception& ex)
    {
      launcher::log::error (categories::steam{}, "failed to create steam_appid.txt: {}", ex.what ());
      co_return false;
    }

    // Our Steam helper uses the Steam API as well and runs in the launcher
    // directory.
    //
    try
    {
      co_await manager_.create_steam_appid (ld, e.appid);

      if (verbose_)
        launcher::log::info (categories::steam{}, "created steam_appid.txt in launcher directory");
    }
    catch (const exception& ex)
    {
      // Without it only the probe below fails, so we carry on.
      //
      launcher::log::warning (categories::steam{},
                              "failed to create steam_appid.txt in launcher directory: {}",
                              ex.what ());
    }

    // The helper initializes the Steam API through Proton, which only
    // succeeds if Proton works and Steam is running.
    //
    fs::path h (gd / "steam.exe");

    if (!fs::exists (h))
    {
      launcher::log::warning (categories::steam{},
                              "steam.exe helper not found at {}, assuming Steam is not running",
                              h.string ());
      co_return false;
    }

    if (verbose_)
      launcher::log::info (categories::steam{}, "running ghost process to check steam status");

    auto r (co_await manager_.run_ghost_process (e, h));

    if (r == ghost_result::steam_running)
    {
      if (verbose_)
        launcher::log::info (categories::steam{}, "steam is running and initialized");

      co_return true;
    }
    else if (r == ghost_result::steam_not_running)
    {
      if (verbose_)
        launcher::log::info (categories::steam{}, "steam is not running");

      co_return false;
    }
    else
    {
      launcher::log::error (categories::steam{}, "failed to check steam status");
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
      launcher::log::info (categories::steam{},
                           "launching through proton, executable: {}, proton: {}",
                           x.string (), e.proton_bin.string ());

      if (!as.empty ())
      {
        string args;
        for (const auto& a : as) args += " " + a;
        launcher::log::info (categories::steam{}, "  arguments:{}", args);
      }
    }

    co_return co_await manager_.launch_through_proton (e, x, as);
  }

  asio::awaitable<bool> proton_coordinator::
  complete_launch (const fs::path& sp,
                   const fs::path& x,
                   uint32_t id,
                   const vector<string>& as)
  {
    if (verbose_)
      launcher::log::info (categories::steam{}, "detecting proton versions");

    auto p (co_await find_best_version (sp));

    if (!p)
    {
      launcher::log::error (categories::steam{}, "no suitable proton version found");
      co_return false;
    }

    if (verbose_)
      launcher::log::info (categories::steam{}, "using proton: {}", p->name);

    auto e (prepare_environment (sp, *p, id));

    fs::path gd (x.parent_path ());
    fs::path ld (fs::current_path ());

    // The Steam client only loads the Companion module at startup. So unless
    // it is already active, we (re)start Steam through Companion, which asks
    // a running client to exit first. That is impossible if Steam itself
    // started us (for example, through the library shortcut), since its
    // shutdown would take us down with it.
    //
    if (companion_installed (gd) && !companion_active ())
    {
      if (getenv ("SteamGameId") != nullptr)
        launcher::log::warning (categories::steam{},
                                "steam started the launcher without companion, so steam friends will not see the game being played");
      else if (!co_await start_steam (gd))
        launcher::log::warning (categories::steam{},
                                "failed to start steam with companion, steam friends will not see the game being played");
    }

    bool r (co_await setup_for_launch (e, gd, ld));

    if (!r)
    {
      // Start Steam, which the game requires, and check again.
      //
      if (verbose_)
        launcher::log::info (categories::steam{},
                             "steam is not running, attempting to start Steam");

      if (!co_await start_steam (gd))
      {
        launcher::log::warning (categories::steam{},
                                "failed to start steam. launching anyway, steam features may not work");
      }
      else
      {
        // Give Steam some time to initialize before we check again.
        //
        if (verbose_)
          launcher::log::info (categories::steam{}, "waiting for steam to initialize");

        asio::steady_timer t (ioc_, 5s);
        co_await t.async_wait (asio::use_awaitable);

        r = (co_await setup_for_launch (e, gd, ld));

        if (r && verbose_)
          launcher::log::info (categories::steam{}, "steam is now running");
      }
    }

    co_return co_await launch (e, x, as);
  }

  asio::awaitable<bool> proton_coordinator::
  is_steam_running (const environment_type& e,
                    const fs::path& h)
  {
    auto r (co_await manager_.run_ghost_process (e, h));
    co_return r == ghost_result::steam_running;
  }

  asio::awaitable<bool> proton_coordinator::
  start_steam (const fs::path& gd)
  {
    bool c (companion_installed (gd));

    try
    {
      // Find Steam in PATH, the same way as Companion does.
      //
      if (c)
        start_detached (companion_program (gd).string (), {"--steam"});
      else
        start_detached ("steam", {});

      if (verbose_)
        launcher::log::info (categories::steam{},
                             c ? "steam started with companion" : "steam started");

      if (!c)
      {
        // Give the client a moment to start. Note that it takes longer to
        // initialize, which the caller waits for.
        //
        asio::steady_timer t (ioc_, 3s);
        co_await t.async_wait (asio::use_awaitable);

        co_return true;
      }

      // Companion waits up to two minutes for a running client to exit
      // before it starts it again with the module. Wait for the module to
      // become active, allowing some more time for the client to start.
      //
      auto d (chrono::steady_clock::now () + 180s);

      for (asio::steady_timer t (ioc_); !companion_active (); )
      {
        if (chrono::steady_clock::now () >= d)
        {
          launcher::log::error (categories::steam{},
                                "companion did not become active in steam");
          co_return false;
        }

        t.expires_after (500ms);
        co_await t.async_wait (asio::use_awaitable);
      }

      launcher::log::info (categories::steam{}, "companion is active in steam");
      co_return true;
    }
    catch (const exception& ex)
    {
      launcher::log::error (categories::steam{}, "failed to start steam: {}", ex.what ());
      co_return false;
    }
  }

  proton_coordinator::manager_type& proton_coordinator::
  manager ()
  {
    return manager_;
  }

  const proton_coordinator::manager_type& proton_coordinator::
  manager () const
  {
    return manager_;
  }
}
