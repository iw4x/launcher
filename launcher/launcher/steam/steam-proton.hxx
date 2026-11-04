// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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

  // Outcome of looking for Proton.
  //
  // @@ Nothing uses this enumeration.
  //
  enum class proton_status
  {
    not_found,       // Not installed.
    found,           // Installed and usable.
    incompatible     // Installed but unusable.
  };

  std::string
  to_string (proton_status);

  // Proton installation in steamapps/common/.
  //
  // The version is the one in the directory name (e.g., 9.0 for Proton
  // 9.0), experimental for Proton - Experimental, and the name itself if
  // we cannot parse it.
  //
  struct proton_version
  {
    fs::path path;           // Installation directory.
    std::string name;        // Directory name.
    std::string version;
    bool experimental;

    proton_version () = default;

    proton_version (fs::path p, std::string n)
      : path (std::move (p)), name (std::move (n)),
        experimental (false) {}
  };

  // Environment for running a program through Proton.
  //
  struct proton_environment
  {
    fs::path steam_root;               // Steam installation directory.
    fs::path compatdata_path;          // Wine prefix.
    fs::path client_install_path;      // Steam client directory.
    fs::path proton_bin;               // The proton script.
    std::uint32_t appid;
    bool enable_logging;
    fs::path log_dir;                  // If logging is enabled.

    proton_environment ()
      : appid (0), enable_logging (false) {}

    // Return the variables to add to the environment of a program run
    // through Proton.
    //
    // @@ SteamAppId and SteamGameId are always 10190, whatever appid is.
    //
    std::map<std::string, std::string>
    build_env_map () const;
  };

  // Our Steam helper (see proton_manager::run_ghost_process()) is a probe
  // executable and the Steam API library, which its releases publish as
  // steam.exe and steam_api64.dll.
  //
  // We install the probe as iw4x-steam.exe since Proton runs every program
  // through its own steam.exe, which takes a program of that name for
  // itself and runs the program's first argument instead.
  //
  constexpr const char* steam_helper_probe = "iw4x-steam.exe";

  // Return the name to install the helper's release asset under in the game
  // directory or nullopt if the asset is not part of the helper.
  //
  std::optional<std::string>
  steam_helper_file (const std::string& asset);

  // Outcome of the Steam probe (see run_ghost_process()).
  //
  enum class ghost_result
  {
    steam_running,      // Steam is running and its API initializes.
    steam_not_running,
    error               // We could not run the probe.
  };

  std::string
  to_string (ghost_result);

  // Return the outcome that the exit status of the Steam probe reports. The
  // probe exits with 0 only if it initializes the Steam API, which is all
  // that we go by since Proton doesn't pass the output of the program it
  // runs through.
  //
  ghost_result
  to_ghost_result (int exit_status);

  // Proton installations of a Steam library and the programs that we run
  // through them.
  //
  class proton_manager
  {
  public:
    // If force_steam_runtime is true, then run programs in the Steam Linux
    // Runtime container, which we otherwise only do on the Steam Deck.
    //
    explicit
    proton_manager (asio::io_context& ioc, bool force_steam_runtime = false);

    proton_manager (const proton_manager&) = delete;
    proton_manager& operator= (const proton_manager&) = delete;

    // Return the Proton installations of the Steam installed in steam_path,
    // best first (see version_compare()).
    //
    asio::awaitable<std::vector<proton_version>>
    detect_proton_versions (fs::path steam_path);

    // Return the best Proton installation if there is any and nullopt
    // otherwise.
    //
    asio::awaitable<std::optional<proton_version>>
    find_best_proton (fs::path steam_path);

    // Return the environment for running the app through proton. If
    // enable_logging is true, then Proton logs into proton_logs/ in the
    // current directory.
    //
    proton_environment
    build_environment (const fs::path& steam_path,
                       const proton_version& proton,
                       std::uint32_t appid,
                       bool enable_logging = false);

    // Write the app id into steam_appid.txt in directory. The Steam API
    // reads it to find out which app it runs as. Throw std::runtime_error
    // on failures.
    //
    asio::awaitable<void>
    create_steam_appid (fs::path directory, std::uint32_t appid);

    // Find out if Steam is running by having our Steam helper, run through
    // Proton, initialize the Steam API.
    //
    // On the Steam Deck we look for the steam process instead, starting
    // Steam if there is none. Note that we exit if Steam doesn't start.
    //
    asio::awaitable<ghost_result>
    run_ghost_process (proton_environment env,
                       fs::path steam_helper_exe);

    // Start the executable through Proton, detached from us and in its
    // directory, passing the arguments. On the Steam Deck, and if
    // force_steam_runtime is true, run it in the Steam Linux Runtime
    // container. Return false if we could not start it.
    //
    asio::awaitable<bool>
    launch_through_proton (proton_environment env,
                           fs::path executable,
                           std::vector<std::string> args = {});

  private:
    // Return the version in a Proton directory name, experimental for
    // Proton - Experimental, and nullopt if there is none.
    //
    std::optional<std::string>
    parse_version (const std::string& name);

    // Return true if a is preferable to b.
    //
    static bool
    version_compare (const proton_version& a, const proton_version& b);

    bool force_steam_runtime_;

    asio::io_context& ioc_;
  };
}
