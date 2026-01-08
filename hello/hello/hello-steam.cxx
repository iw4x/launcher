#include <hello/hello-steam.hxx>

#include <vector>
#include <string>
#include <algorithm>

using namespace std;

namespace hello
{
  steam_coordinator::
  steam_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      manager_ (make_unique<steam_library_manager> (ioc)),
      initialized_ (false)
  {
  }

  asio::awaitable<bool> steam_coordinator::
  initialize ()
  {
    // If we are already up and running, short-circuit.
    //
    if (initialized_)
      co_return true;

    // Try to sniff out the Steam installation path. If we find it, the
    // manager is ready to roll.
    //
    auto p (co_await manager_->detect_steam_path ());
    initialized_ = p.has_value ();

    co_return initialized_;
  }

  asio::awaitable<bool> steam_coordinator::
  is_available ()
  {
    if (!initialized_)
      co_await initialize ();

    co_return initialized_;
  }

  // App lookups.
  //
  // MW2 has split personalities: 10190 (MP) and 10180 (SP). While they
  // usually live together in the same directory, we care about MP (10190)
  // because that's what IW4x hooks into.
  //

  asio::awaitable<optional<fs::path>> steam_coordinator::
  find_mw2_multiplayer ()
  {
    co_return co_await find_app (steam_appid::mw2_multiplayer);
  }

  asio::awaitable<optional<fs::path>> steam_coordinator::
  find_mw2_singleplayer ()
  {
    co_return co_await find_app (steam_appid::mw2_singleplayer);
  }

  asio::awaitable<optional<fs::path>> steam_coordinator::
  find_mw2 ()
  {
    // Prioritize MP. If that's missing, we could try SP, but honestly if MP
    // isn't there, IW4x probably won't be happy anyway.
    //
    auto mp (co_await find_mw2_multiplayer ());

    if (mp)
      co_return mp;

    co_return nullopt;
  }

  // App Manifests.
  //

  asio::awaitable<optional<steam_coordinator::manifest_type>> steam_coordinator::
  get_mw2_multiplayer_manifest ()
  {
    co_return co_await get_app_manifest (steam_appid::mw2_multiplayer);
  }

  asio::awaitable<optional<steam_coordinator::manifest_type>> steam_coordinator::
  get_mw2_singleplayer_manifest ()
  {
    co_return co_await get_app_manifest (steam_appid::mw2_singleplayer);
  }

  asio::awaitable<vector<steam_coordinator::library_type>> steam_coordinator::
  get_libraries ()
  {
    if (!initialized_)
      co_await initialize ();

    co_return co_await manager_->load_libraries ();
  }

  optional<fs::path> steam_coordinator::
  steam_path () const
  {
    return manager_->cached_steam_path ();
  }

  bool steam_coordinator::
  validate_mw2_path (const fs::path& p)
  {
    if (!fs::exists (p) || !fs::is_directory (p))
      return false;

    // Check for common MW2 artifacts.
    //
    // Note that we accept the path if *any* of these are found. This is perhaps
    // a bit lenient, but it accounts for partial installs or dedicated server
    // setups.
    //
    static const char* expected[] =
    {
      "iw4mp.exe",      // Windows MP executable.
      "iw4sp.exe",      // Windows SP executable.
      "iw4x.exe",       // IW4x client (if already present).
      "main",           // Main asset directory.
      "zone",           // Zone files.
      "players"         // Profiles.
    };

    for (const char* f : expected)
    {
      if (fs::exists (p / f))
        return true;
    }

    return false;
  }

  asio::awaitable<optional<fs::path>> steam_coordinator::
  get_default_mw2_path ()
  {
    // First, ask Steam.
    //
    auto sp (co_await find_mw2 ());

    if (sp)
      co_return sp;

    // If Steam came up empty (or we couldn't find Steam itself), we are done.
    // We could try poking around the Registry or Program Files, but that's a
    // can of worms for another day.
    //
    co_return nullopt;
  }

  // Internal helpers.
  //

  asio::awaitable<optional<fs::path>> steam_coordinator::
  find_app (uint32_t id)
  {
    if (!initialized_)
      co_await initialize ();

    if (!initialized_)
      co_return nullopt;

    co_return co_await manager_->find_app (id);
  }

  asio::awaitable<optional<steam_coordinator::manifest_type>> steam_coordinator::
  get_app_manifest (uint32_t id)
  {
    if (!initialized_)
      co_await initialize ();

    if (!initialized_)
      co_return nullopt;

    co_return co_await manager_->load_app_manifest (id);
  }

  // Standalone wrappers.
  //
  // These are for one-off lookups where the caller doesn't want to manage the
  // coordinator lifecycle manually.
  //

  asio::awaitable<optional<fs::path>>
  locate_mw2 (asio::io_context& ioc)
  {
    steam_coordinator c (ioc);
    co_await c.initialize ();
    co_return co_await c.find_mw2 ();
  }

  asio::awaitable<optional<fs::path>>
  get_mw2_default_path (asio::io_context& ioc)
  {
    steam_coordinator c (ioc);
    co_await c.initialize ();
    co_return co_await c.get_default_mw2_path ();
  }
}
