#pragma once

#include <launcher/steam/steam-cm-client.hxx>
#include <launcher/steam/steam-crypto.hxx>

#include <boost/asio/awaitable.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace launcher
{
  struct steam_content_server
  {
    std::string host;
    std::string vhost;

    std::string type;

    std::uint16_t port  = 443;
    bool          https = true;

    std::int32_t load          = 0;
    float        weighted_load = 0.0f;

    std::vector<std::uint32_t> allowed_app_ids;

    std::string
    base_url () const;

    bool
    serves (std::uint32_t app_id) const noexcept;
  };

  class steam_content_client
  {
  public:
    explicit
    steam_content_client (steam_cm_client&);

    steam_content_client (const steam_content_client&) = delete;
    steam_content_client& operator= (const steam_content_client&) = delete;

    asio::awaitable<std::vector<steam_content_server>>
    fetch_servers (std::uint32_t cell_id,
                   std::uint32_t app_id      = 0,
                   std::uint32_t max_servers = 20);

    asio::awaitable<std::uint64_t>
    manifest_request_code (std::uint32_t app_id,
                           std::uint32_t depot_id,
                           std::uint64_t manifest_id,
                           const std::string& branch = "public");

    asio::awaitable<crypto::aes_key>
    depot_key (std::uint32_t app_id, std::uint32_t depot_id);

    asio::awaitable<std::string>
    cdn_auth_token (std::uint32_t app_id,
                    std::uint32_t depot_id,
                    const std::string& host);

  private:
    steam_cm_client& cm_;
  };
}
