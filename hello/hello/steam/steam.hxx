#pragma once

#include <hello/steam/steam-types.hxx>
#include <hello/steam/steam-parser.hxx>
#include <hello/steam/steam-library.hxx>

namespace hello
{
  // Convenience functions for common Steam operations.
  //

  // Quick check if Steam is installed on this system.
  //
  asio::awaitable<bool>
  is_steam_installed (asio::io_context& ioc);

  // Get Steam installation path without creating a manager instance.
  //
  asio::awaitable<std::optional<fs::path>>
  get_steam_path (asio::io_context& ioc);

  // Find a Steam game by App ID without creating a manager instance.
  //
  asio::awaitable<std::optional<fs::path>>
  find_steam_game (asio::io_context& ioc, std::uint32_t appid);

  // Get all Steam libraries without creating a manager instance.
  //
  asio::awaitable<std::vector<steam_library>>
  get_steam_libraries (asio::io_context& ioc);
}
