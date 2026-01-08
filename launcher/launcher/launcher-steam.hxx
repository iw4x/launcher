#pragma once

#include <launcher/steam/steam.hxx>

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <optional>
#include <vector>

namespace launcher
{
  namespace asio = boost::asio;

  class steam_coordinator
  {
  public:
    using library_type = steam_library;
    using manifest_type = steam_app_manifest;

    // Constructors.
    //
    explicit
    steam_coordinator (asio::io_context& ioc);

    steam_coordinator (const steam_coordinator&) = delete;
    steam_coordinator& operator= (const steam_coordinator&) = delete;

    // Initialize Steam detection.
    //
    // Detects Steam installation and loads library information.
    // Should be called before other operations.
    //
    asio::awaitable<bool>
    initialize ();

    // Check if Steam is available on this system.
    //
    asio::awaitable<bool>
    is_available ();

    // Find MW2 multiplayer installation path.
    //
    // Searches all Steam libraries for Modern Warfare 2 Multiplayer
    // (App ID 10190) and returns the installation path if found.
    //
    asio::awaitable<std::optional<fs::path>>
    find_mw2_multiplayer ();

    // Find MW2 singleplayer installation path.
    //
    // Searches all Steam libraries for Modern Warfare 2 Singleplayer
    // (App ID 10180) and returns the installation path if found.
    //
    asio::awaitable<std::optional<fs::path>>
    find_mw2_singleplayer ();

    // Find any MW2 installation (prefers multiplayer).
    //
    // Tries to find MW2 multiplayer first, then singleplayer if
    // multiplayer is not found.
    //
    asio::awaitable<std::optional<fs::path>>
    find_mw2 ();

    // Get detailed manifest for MW2 multiplayer.
    //
    asio::awaitable<std::optional<manifest_type>>
    get_mw2_multiplayer_manifest ();

    // Get detailed manifest for MW2 singleplayer.
    //
    asio::awaitable<std::optional<manifest_type>>
    get_mw2_singleplayer_manifest ();

    // Get all Steam libraries.
    //
    asio::awaitable<std::vector<library_type>>
    get_libraries ();

    // Get Steam installation path.
    //
    std::optional<fs::path>
    steam_path () const;

    // Validate that a path is a valid MW2 installation.
    //
    // Checks for expected MW2 files and directories.
    //
    static bool
    validate_mw2_path (const fs::path& path);

    // Get the default MW2 installation path.
    //
    // Returns the most likely MW2 installation path based on Steam
    // library detection and validation. This is the recommended method
    // for getting the MW2 path for use as a default.
    //
    asio::awaitable<std::optional<fs::path>>
    get_default_mw2_path ();

  private:
    // Find a specific app by App ID.
    //
    asio::awaitable<std::optional<fs::path>>
    find_app (std::uint32_t appid);

    // Get manifest for a specific app.
    //
    asio::awaitable<std::optional<manifest_type>>
    get_app_manifest (std::uint32_t appid);

    // IO context reference.
    //
    asio::io_context& ioc_;

    // Steam library manager.
    //
    std::unique_ptr<steam_library_manager> manager_;

    // Initialization flag.
    //
    bool initialized_;
  };

  // Standalone convenience function to find MW2 installation.
  //
  // Creates a temporary coordinator and searches for MW2.
  // This is a quick way to locate MW2 without managing a coordinator instance.
  //
  asio::awaitable<std::optional<fs::path>>
  locate_mw2 (asio::io_context& ioc);

  // Standalone convenience function to get default MW2 path.
  //
  // This is the recommended way to get the MW2 installation path
  // for use as a default in the application.
  //
  asio::awaitable<std::optional<fs::path>>
  get_mw2_default_path (asio::io_context& ioc);
}
