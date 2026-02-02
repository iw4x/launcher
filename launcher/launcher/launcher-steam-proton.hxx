#pragma once

#include <launcher/steam/steam-proton.hxx>
#include <launcher/steam/steam-types.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <cstdint>
#include <string>

namespace launcher
{
  namespace fs = std::filesystem;
  namespace asio = boost::asio;

  class proton_coordinator
  {
  public:
    using manager_type = proton_manager;
    using environment_type = proton_environment;
    using version_type = proton_version;

    // Constructors.
    //
    explicit
    proton_coordinator (asio::io_context& ioc);

    proton_coordinator (const proton_coordinator&) = delete;
    proton_coordinator& operator= (const proton_coordinator&) = delete;

    // Configuration.
    //
    void
    set_verbose (bool v);

    bool
    verbose () const;

    void
    set_enable_logging (bool v);

    bool
    enable_logging () const;

    // Proton detection.
    //
    // Detect all available Proton versions in the Steam installation.
    //
    asio::awaitable<std::vector<version_type>>
    detect_versions (const fs::path& steam_path);

    // Find the best Proton version automatically.
    //
    asio::awaitable<std::optional<version_type>>
    find_best_version (const fs::path& steam_path);

    // Environment preparation.
    //
    // Prepare the Proton environment for launching an application.
    // This builds the environment configuration but doesn't create files.
    //
    environment_type
    prepare_environment (const fs::path& steam_path,
                        const version_type& proton,
                        std::uint32_t appid);

    // Setup for launch.
    //
    asio::awaitable<bool>
    setup_for_launch (const environment_type& env,
                      const fs::path& game_directory,
                      const fs::path& launcher_directory);

    // Launch game through Proton.
    //
    // Launches the specified executable through Proton with optional
    // command-line arguments.
    //
    asio::awaitable<bool>
    launch (const environment_type& env,
            const fs::path& executable,
            const std::vector<std::string>& args = {});

    // Complete launch workflow.
    //
    asio::awaitable<bool>
    complete_launch (const fs::path& steam_path,
                     const fs::path& executable,
                     std::uint32_t appid,
                     const std::vector<std::string>& args = {});

    // Check if Steam is running.
    //
    // Uses the ghost process method to check Steam status.
    //
    asio::awaitable<bool>
    is_steam_running (const environment_type& env,
                      const fs::path& steam_helper_exe);

    // Start Steam natively.
    //
    // Attempts to launch the native Steam client.
    //
    asio::awaitable<bool>
    start_steam ();

    // Access to underlying manager.
    //
    manager_type&
    manager ();

    const manager_type&
    manager () const;

  private:
    // IO context reference.
    //
    asio::io_context& ioc_;

    // Underlying Proton manager.
    //
    std::unique_ptr<manager_type> manager_;

    // Configuration flags.
    //
    bool verbose_;
    bool logging_;
  };
}
