#pragma once

#include <launcher/steam/steam-types.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace launcher
{
  namespace fs = std::filesystem;
  namespace asio = boost::asio;

  // Proton detection status.
  //
  enum class proton_status
  {
    not_found,       // Proton not installed
    found,           // Proton found and ready
    incompatible     // Proton found but incompatible
  };

  std::string
  to_string (proton_status);

  // Proton version information.
  //
  struct proton_version
  {
    fs::path path;           // Full path to Proton installation
    std::string name;        // Display name (e.g., "Proton 9.0")
    std::string version;     // Version string (e.g., "9.0")
    bool experimental;       // Is this an experimental version?

    proton_version () = default;

    proton_version (fs::path p, std::string n)
      : path (std::move (p)), name (std::move (n)),
        experimental (false) {}
  };

  // Proton environment configuration.
  //
  struct proton_environment
  {
    fs::path steam_root;               // Steam installation root
    fs::path compatdata_path;          // STEAM_COMPAT_DATA_PATH
    fs::path client_install_path;      // STEAM_COMPAT_CLIENT_INSTALL_PATH
    fs::path proton_bin;               // Path to proton executable
    std::uint32_t appid;               // Steam App ID
    bool enable_logging;               // Enable Proton logging
    fs::path log_dir;                  // Log directory (if logging enabled)

    proton_environment ()
      : appid (0), enable_logging (false) {}

    // Build environment variables map.
    //
    std::map<std::string, std::string>
    build_env_map () const;
  };

  // Proton ghost process result.
  //
  enum class ghost_result
  {
    steam_running,      // Steam is running and initialized
    steam_not_running,  // Steam is not running
    error               // Error occurred during check
  };

  std::string
  to_string (ghost_result);

  // Proton manager for detecting and managing Proton installations.
  //
  class proton_manager
  {
  public:
    // Constructor.
    //
    explicit
    proton_manager (asio::io_context& ioc);

    proton_manager (const proton_manager&) = delete;
    proton_manager& operator= (const proton_manager&) = delete;

    // Detect available Proton versions in Steam.
    //
    // Scans the Steam installation for all available Proton versions
    // and returns them sorted by version (newest first).
    //
    asio::awaitable<std::vector<proton_version>>
    detect_proton_versions (const fs::path& steam_path);

    // Find the best Proton version.
    //
    // Returns the newest/best available Proton version, or nullopt if
    // none found.
    //
    asio::awaitable<std::optional<proton_version>>
    find_best_proton (const fs::path& steam_path);

    // Build Proton environment for a specific app.
    //
    // Creates the environment configuration needed to run an application
    // through Proton.
    //
    proton_environment
    build_environment (const fs::path& steam_path,
                       const proton_version& proton,
                       std::uint32_t appid,
                       bool enable_logging = false);

    // Create steam_appid.txt file.
    //
    // Creates the steam_appid.txt file needed by Steamworks SDK to
    // identify the application.
    //
    asio::awaitable<void>
    create_steam_appid (const fs::path& directory, std::uint32_t appid);

    // Run ghost process to check Steam status.
    //
    // Launches our steam.exe helper through Proton to check if Steam is
    // running and the API can be initialized.
    //
    asio::awaitable<ghost_result>
    run_ghost_process (const proton_environment& env,
                       const fs::path& steam_helper_exe);

    // Launch application through Proton.
    //
    // Launches the specified executable through Proton with the given
    // environment and arguments.
    //
    asio::awaitable<bool>
    launch_through_proton (const proton_environment& env,
                           const fs::path& executable,
                           const std::vector<std::string>& args = {});

  private:
    // Parse version from Proton directory name.
    //
    std::optional<std::string>
    parse_version (const std::string& name);

    // Compare Proton versions for sorting.
    //
    static bool
    version_compare (const proton_version& a, const proton_version& b);

    // IO context reference.
    //
    asio::io_context& ioc_;
  };
}
