#include <launcher/launcher-steam.hxx>

#include <vector>
#include <string>
#include <algorithm>

using namespace std;

namespace launcher
{
  steam_coordinator::
  steam_coordinator (asio::io_context& c)
    : ioc_ (c),
      manager_ (make_unique<steam_library_manager> (c)),
      initialized_ (false)
  {
  }

  asio::awaitable<bool> steam_coordinator::
  initialize ()
  {
    if (initialized_)
      co_return true;

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
    auto mp (co_await find_mw2_multiplayer ());

    if (mp)
      co_return mp;

    co_return nullopt;
  }

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
    // Note that we accept the path if *any* of these are found. This is
    // lenient, but it accounts for partial installs and dedicated server
    // setups.
    //
    static const char* l[] =
    {
      "iw4mp.exe",
      "iw4sp.exe",
      "iw4x.exe",
      "main",
      "zone",
      "players"
    };

    for (const char* f: l)
    {
      if (fs::exists (p / f))
        return true;
    }

    return false;
  }

  asio::awaitable<optional<fs::path>> steam_coordinator::
  get_default_mw2_path ()
  {
    auto p (co_await find_mw2 ());

    if (p)
      co_return p;

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

  // One-off lookup wrappers.
  //

  asio::awaitable<optional<fs::path>>
  locate_mw2 (asio::io_context& c)
  {
    steam_coordinator sc (c);
    co_await sc.initialize ();
    co_return co_await sc.find_mw2 ();
  }

  asio::awaitable<optional<fs::path>>
  get_mw2_default_path (asio::io_context& c)
  {
    steam_coordinator sc (c);
    co_await sc.initialize ();
    co_return co_await sc.get_default_mw2_path ();
  }
}
