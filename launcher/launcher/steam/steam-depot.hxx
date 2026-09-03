#pragma once

#include <launcher/http/http-client.hxx>
#include <launcher/launcher-progress.hxx>
#include <launcher/steam/steam-content.hxx>
#include <launcher/steam/steam-manifest.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace launcher
{
  namespace fs = std::filesystem;

  struct steam_depot_target
  {
    std::uint32_t app_id      = 0;
    std::uint32_t depot_id    = 0;
    std::uint64_t manifest_id = 0;

    std::string branch = "public";

    bool
    valid () const noexcept
    {
      return app_id != 0 && depot_id != 0 && manifest_id != 0;
    }

    std::string
    to_string () const;
  };

  struct steam_depot_traits
  {
    std::size_t parallel_chunks = 8;

    std::size_t chunk_attempts = 4;

    std::uint32_t max_servers = 20;

    bool verify_existing = true;

    bool force_download = false;

    std::chrono::milliseconds request_timeout {60000};
  };

  struct steam_depot_result
  {
    std::uint64_t bytes_downloaded = 0;
    std::uint64_t bytes_written    = 0;

    std::uint64_t bytes_skipped = 0;

    std::size_t files_written  = 0;
    std::size_t files_skipped  = 0;
    std::size_t chunks_fetched = 0;

    std::chrono::milliseconds elapsed {0};
  };

  class steam_depot_installer
  {
  public:
    steam_depot_installer (asio::io_context&,
                           steam_content_client&,
                           http_client_traits,
                           steam_depot_traits = {});

    steam_depot_installer (const steam_depot_installer&) = delete;
    steam_depot_installer& operator= (const steam_depot_installer&) = delete;

    asio::awaitable<steam_depot_manifest>
    fetch_manifest (const steam_depot_target&, std::uint32_t cell_id = 0);

    asio::awaitable<steam_depot_result>
    install (const steam_depot_target& target,
             const fs::path& root,
             progress_coordinator* progress,
             std::uint32_t cell_id = 0);

  private:
    asio::awaitable<steam_depot_manifest>
    fetch_manifest_with_key (const steam_depot_target&,
                             const crypto::aes_key&,
                             std::uint32_t cell_id);

    struct file_plan
    {
      const steam_depot_file* file = nullptr;

      bool download = true;
    };

    struct plan
    {
      std::vector<file_plan> files;

      std::uint64_t bytes_to_download = 0;
      std::uint64_t bytes_present     = 0;
      std::size_t   chunks_to_fetch   = 0;
    };

    plan
    build_plan (const steam_depot_manifest&, const fs::path& root) const;

    bool
    already_installed (const steam_depot_file&, const fs::path& path) const;

    asio::awaitable<crypto::byte_buffer>
    fetch_chunk (const steam_depot_chunk&,
                 std::uint32_t depot_id,
                 const crypto::aes_key&);

    const steam_content_server&
    next_server ();

    asio::awaitable<std::uint64_t>
    install_file (const steam_depot_file&,
                  const fs::path& path,
                  std::uint32_t depot_id,
                  const crypto::aes_key&,
                  steam_depot_result&,
                  const std::function<void (std::uint64_t)>& on_bytes);

  private:
    asio::io_context&     ioc_;
    steam_content_client& content_;
    http_client_traits    http_traits_;
    steam_depot_traits    traits_;

    std::vector<steam_content_server> servers_;
    std::size_t                       server_cursor_ = 0;

    std::string cdn_token_;

    std::unique_ptr<http_client> http_;
  };
}
