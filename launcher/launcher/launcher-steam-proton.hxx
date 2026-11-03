// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <launcher/steam/steam-proton.hxx>
#include <launcher/steam/steam-types.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <filesystem>
#include <optional>
#include <cstdint>
#include <string>

namespace launcher
{
  namespace fs = std::filesystem;
  namespace asio = boost::asio;

  // Launch of a client through Proton with the native Steam client running,
  // which the client's Steam API requires.
  //
  class proton_coordinator
  {
  public:
    using manager_type = proton_manager;
    using environment_type = proton_environment;
    using version_type = proton_version;

    // If force_steam_runtime is true, then run the client in the Steam Linux
    // Runtime container (see proton_manager::launch_through_proton()).
    //
    explicit
    proton_coordinator (asio::io_context& ioc, bool force_steam_runtime);

    proton_coordinator (const proton_coordinator&) = delete;
    proton_coordinator& operator= (const proton_coordinator&) = delete;

    // In the verbose mode we log each step of the launch.
    //
    void
    set_verbose (bool v);

    bool
    verbose () const;

    // Have Proton write its logs (see proton_manager::build_environment()).
    //
    void
    set_enable_logging (bool v);

    bool
    enable_logging () const;

    // Return the Proton installations of the Steam installation, best first.
    //
    asio::awaitable<std::vector<version_type>>
    detect_versions (fs::path steam_path);

    // Return the best Proton installation, if any.
    //
    asio::awaitable<std::optional<version_type>>
    find_best_version (fs::path steam_path);

    // Return the environment for running the app through the Proton
    // installation. Note that this doesn't write anything.
    //
    environment_type
    prepare_environment (const fs::path& steam_path,
                        const version_type& proton,
                        std::uint32_t appid);

    // Write steam_appid.txt into the game and launcher directories and
    // return true if Steam is running, which we find out with our Steam
    // helper in the game directory (see proton_manager::run_ghost_process()).
    // Otherwise, including on failures, return false.
    //
    asio::awaitable<bool>
    setup_for_launch (environment_type env,
                      fs::path game_directory,
                      fs::path launcher_directory);

    // Start the executable through Proton, passing it the arguments. Return
    // false if we could not start it.
    //
    asio::awaitable<bool>
    launch (environment_type env,
            fs::path executable,
            std::vector<std::string> args = {});

    // Launch the executable through the best Proton installation, starting
    // Steam first if it isn't running. If Companion is installed, then also
    // make sure Steam runs with its module, restarting Steam if necessary.
    //
    asio::awaitable<bool>
    complete_launch (fs::path steam_path,
                     fs::path executable,
                     std::uint32_t appid,
                     std::vector<std::string> args = {});

    // Return true if our Steam helper finds Steam running (see
    // proton_manager::run_ghost_process()).
    //
    asio::awaitable<bool>
    is_steam_running (environment_type env,
                      fs::path steam_helper_exe);

    // Start the native Steam client, through Companion if the game directory
    // has it, in which case wait for its module to become active. Return
    // false if Steam could not be started.
    //
    asio::awaitable<bool>
    start_steam (fs::path game_directory);

    manager_type&
    manager ();

    const manager_type&
    manager () const;

  private:
    asio::io_context& ioc_;
    manager_type manager_;

    bool verbose_;
    bool logging_;
  };
}
