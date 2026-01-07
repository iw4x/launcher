#include <hello/hello-steam.hxx>

#include <vector>
#include <string>
#include <algorithm>

namespace hello
{
  steam_coordinator::
  steam_coordinator (asio::io_context& ioc)
      : ioc_ (ioc),
        manager_ (std::make_unique<steam_library_manager> (ioc)),
        initialized_ (false)
  {
  }

  asio::awaitable<bool> steam_coordinator::
  initialize ()
  {
    // If we are already initialized, there is nothing to do.
    //
    if (initialized_)
      co_return true;

    // Try to detect the Steam installation path. If successful, the manager
    // is ready to serve requests.
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
  // Note that Modern Warfare 2 technically has two App IDs: 10190 for the
  // multiplayer component and 10180 for singleplayer. For IW4x purposes, we
  // generally care about the multiplayer installation, though they almost
  // always reside in the same physical directory.
  //

  asio::awaitable<std::optional<fs::path>> steam_coordinator::
  find_mw2_multiplayer ()
  {
    co_return co_await find_app (steam_appid::mw2_multiplayer);
  }

  asio::awaitable<std::optional<fs::path>> steam_coordinator::
  find_mw2_singleplayer ()
  {
    co_return co_await find_app (steam_appid::mw2_singleplayer);
  }

  asio::awaitable<std::optional<fs::path>> steam_coordinator::
  find_mw2 ()
  {
    // We prioritize the multiplayer App ID as that is the base for IW4x.
    //
    auto mp (co_await find_mw2_multiplayer ());

    if (mp)
      co_return mp;

    co_return std::nullopt;
  }

  // App Manifests.
  //

  asio::awaitable<std::optional<steam_coordinator::manifest_type>> steam_coordinator::
  get_mw2_multiplayer_manifest ()
  {
    co_return co_await get_app_manifest (steam_appid::mw2_multiplayer);
  }

  asio::awaitable<std::optional<steam_coordinator::manifest_type>> steam_coordinator::
  get_mw2_singleplayer_manifest ()
  {
    co_return co_await get_app_manifest (steam_appid::mw2_singleplayer);
  }

  asio::awaitable<std::vector<steam_coordinator::library_type>> steam_coordinator::
  get_libraries ()
  {
    if (!initialized_)
      co_await initialize ();

    co_return co_await manager_->load_libraries ();
  }

  std::optional<fs::path> steam_coordinator::
  steam_path () const
  {
    return manager_->cached_steam_path ();
  }

  bool steam_coordinator::
  validate_mw2_path (const fs::path& p)
  {
    if (!fs::exists (p) || !fs::is_directory (p))
      return false;

    // Check for common MW2 files/directories.
    //
    // Note that we accept the path if *any* of these are found. This is perhaps
    // a bit lenient, but it accounts for partial installs or dedicated server
    // setups.
    //
    static const std::vector<std::string> expected =
    {
      "iw4mp.exe",      // Windows multiplayer executable.
      "iw4sp.exe",      // Windows singleplayer executable.
      "iw4x.exe",       // IW4x client (if already installed).
      "main",           // Main game directory.
      "zone",           // Zone files directory.
      "players"         // Player profiles directory.
    };

    for (const auto& f : expected)
    {
      if (fs::exists (p / f))
        return true;
    }

    return false;
  }

  asio::awaitable<std::optional<fs::path>> steam_coordinator::
  get_default_mw2_path ()
  {
    // First, try to locate the game via Steam integration.
    //
    auto steam_p (co_await find_mw2 ());

    if (steam_p)
      co_return steam_p;

    // If we couldn't find it via Steam (e.g., non-Steam install or Steam is
    // not detected), we could try checking the Windows Registry or common
    // locations, but for now we return empty.
    //
    co_return std::nullopt;
  }

  // Internal helpers.
  //

  asio::awaitable<std::optional<fs::path>> steam_coordinator::
  find_app (std::uint32_t appid)
  {
    if (!initialized_)
      co_await initialize ();

    if (!initialized_)
      co_return std::nullopt;

    co_return co_await manager_->find_app (appid);
  }

  asio::awaitable<std::optional<steam_coordinator::manifest_type>> steam_coordinator::
  get_app_manifest (std::uint32_t appid)
  {
    if (!initialized_)
      co_await initialize ();

    if (!initialized_)
      co_return std::nullopt;

    co_return co_await manager_->load_app_manifest (appid);
  }

  // Standalone wrappers.
  //
  // These allow one-off lookups without manually managing the coordinator
  // lifecycle.
  //

  asio::awaitable<std::optional<fs::path>>
  locate_mw2 (asio::io_context& ioc)
  {
    steam_coordinator c (ioc);
    co_await c.initialize ();
    co_return co_await c.find_mw2 ();
  }

  asio::awaitable<std::optional<fs::path>>
  get_mw2_default_path (asio::io_context& ioc)
  {
    steam_coordinator c (ioc);
    co_await c.initialize ();
    co_return co_await c.get_default_mw2_path ();
  }
}
