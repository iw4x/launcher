#pragma once

#include <launcher/steam/steam-types.hxx>
#include <launcher/steam/steam-parser.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <string>
#include <vector>
#include <optional>
#include <map>

namespace launcher
{
  namespace asio = boost::asio;

  class steam_library_manager
  {
  public:
    // Constructor.
    //
    explicit
    steam_library_manager (asio::io_context& ioc);

    steam_library_manager (const steam_library_manager&) = delete;
    steam_library_manager& operator= (const steam_library_manager&) = delete;

    // Detect Steam installation path for the current platform.
    //
    // Returns the main Steam installation directory, or std::nullopt if
    // Steam is not installed or cannot be found.
    //
    asio::awaitable<std::optional<fs::path>>
    detect_steam_path ();

    // Get Steam configuration paths.
    //
    // Returns paths to important Steam configuration files and directories.
    // Requires that detect_steam_path() has been called successfully.
    //
    asio::awaitable<steam_config_paths>
    get_config_paths ();

    // Load all Steam library folders.
    //
    // Reads and parses the libraryfolders.vdf file to get all configured
    // Steam library locations.
    //
    asio::awaitable<std::vector<steam_library>>
    load_libraries ();

    // Find a specific app by App ID across all libraries.
    //
    // Searches all Steam libraries for the specified App ID and returns
    // the full installation path if found.
    //
    asio::awaitable<std::optional<fs::path>>
    find_app (std::uint32_t appid);

    // Load app manifest for a specific App ID.
    //
    // Reads the appmanifest_<appid>.acf file and returns detailed
    // information about the installed application.
    //
    asio::awaitable<std::optional<steam_app_manifest>>
    load_app_manifest (std::uint32_t appid);

    // Get all installed apps across all libraries.
    //
    // Returns a map of App ID to installation path for all installed apps.
    //
    asio::awaitable<std::map<std::uint32_t, fs::path>>
    get_all_apps ();

    // Validate that a path is a valid Steam library.
    //
    // Checks if the specified path contains the expected Steam library
    // structure (steamapps directory, etc.).
    //
    static bool
    validate_library_path (const fs::path& path);

    // Get the cached Steam installation path.
    //
    // Returns the Steam path from the last successful detect_steam_path()
    // call, or std::nullopt if not yet detected.
    //
    std::optional<fs::path>
    cached_steam_path () const
    {
      return steam_path_;
    }

  private:
    // Detect Steam path on Linux.
    //
    asio::awaitable<std::optional<fs::path>>
    detect_steam_path_linux ();

    // Detect Steam path on Windows.
    //
    asio::awaitable<std::optional<fs::path>>
    detect_steam_path_windows ();

    // Detect Steam path on macOS.
    //
    asio::awaitable<std::optional<fs::path>>
    detect_steam_path_macos ();

    // Find app manifest file in a library.
    //
    std::optional<fs::path>
    find_app_manifest_file (const steam_library& lib, std::uint32_t appid);

    // IO context reference.
    //
    asio::io_context& ioc_;

    // Cached Steam installation path.
    //
    std::optional<fs::path> steam_path_;

    // Cached libraries.
    //
    std::vector<steam_library> libraries_;
    bool libraries_loaded_;
  };
}
