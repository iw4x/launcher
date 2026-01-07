#include <hello/steam/steam.hxx>

namespace hello
{
  asio::awaitable<bool>
  is_steam_installed (asio::io_context& ioc)
  {
    steam_library_manager manager (ioc);
    auto path = co_await manager.detect_steam_path ();
    co_return path.has_value ();
  }

  asio::awaitable<std::optional<fs::path>>
  get_steam_path (asio::io_context& ioc)
  {
    steam_library_manager manager (ioc);
    co_return co_await manager.detect_steam_path ();
  }

  asio::awaitable<std::optional<fs::path>>
  find_steam_game (asio::io_context& ioc, std::uint32_t appid)
  {
    steam_library_manager manager (ioc);
    co_await manager.detect_steam_path ();
    co_return co_await manager.find_app (appid);
  }

  asio::awaitable<std::vector<steam_library>>
  get_steam_libraries (asio::io_context& ioc)
  {
    steam_library_manager manager (ioc);
    co_await manager.detect_steam_path ();
    co_return co_await manager.load_libraries ();
  }
}
