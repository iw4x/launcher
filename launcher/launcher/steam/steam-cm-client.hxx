#pragma once

#include <launcher/http/http-client.hxx>
#include <launcher/protobuf/protobuf-wire.hxx>
#include <launcher/steam/steam-message.hxx>
#include <launcher/steam/steam-result.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace launcher
{
  namespace asio = boost::asio;
  namespace beast = boost::beast;
  namespace ssl = boost::asio::ssl;

  struct steam_cm_endpoint
  {
    std::string   host;
    std::uint16_t port = 0;

    std::string realm;

    std::int32_t load          = 0;
    double       weighted_load = 0.0;

    std::string
    to_string () const;
  };

  struct steam_cm_traits
  {
    std::chrono::milliseconds connect_timeout {15000};

    std::chrono::milliseconds request_timeout {30000};

    std::chrono::seconds heartbeat_interval {9};

    std::size_t max_message_size = 8 * 1024 * 1024;

    std::size_t max_connect_attempts = 5;

    bool        verify_ssl = false;
    std::string proxy_url;
  };

  class steam_cm_client
  {
  public:
    steam_cm_client (asio::io_context&, steam_cm_traits = {});

    ~steam_cm_client ();

    steam_cm_client (const steam_cm_client&) = delete;
    steam_cm_client& operator= (const steam_cm_client&) = delete;

    static asio::awaitable<std::vector<steam_cm_endpoint>>
    discover_endpoints (asio::io_context&,
                        const http_client_traits&,
                        std::uint32_t cell_id = 0);

    asio::awaitable<void>
    connect (const steam_cm_endpoint&);

    asio::awaitable<void>
    connect_any (std::uint32_t cell_id = 0);

    asio::awaitable<void>
    disconnect ();

    bool
    connected () const noexcept
    {
      return conn_ != nullptr && conn_->open;
    }

    asio::awaitable<void>
    log_on (std::uint64_t steam_id,
            const std::string& refresh_token,
            std::uint32_t cell_id = 0);

    std::optional<std::uint64_t>
    steam_id () const noexcept
    {
      return steam_id_;
    }

    std::optional<std::int32_t>
    session_id () const noexcept
    {
      return session_id_;
    }

    bool
    logged_on () const noexcept
    {
      return session_id_.has_value () && connected ();
    }

    const std::vector<std::uint32_t>&
    licenses () const noexcept
    {
      return licenses_;
    }

    asio::awaitable<bool>
    await_licenses (std::chrono::milliseconds timeout);

    asio::awaitable<steam_message>
    send_job (steam_emsg, pb::byte_buffer body);

    asio::awaitable<void>
    send (steam_emsg, pb::byte_buffer body);

    asio::awaitable<pb::byte_buffer>
    call_service (const std::string& method, pb::byte_buffer request);

  private:
    using tcp_stream       = beast::tcp_stream;
    using tls_stream       = beast::ssl_stream<tcp_stream>;
    using websocket_stream = beast::websocket::stream<tls_stream>;

    struct pending_job
    {
      explicit
      pending_job (asio::io_context& c)
        : event (c) {}

      asio::steady_timer           event;
      std::optional<steam_message> response;
      std::exception_ptr           error;
    };

    struct connection
    {
      explicit
      connection (asio::io_context&, ssl::context&);

      websocket_stream ws;

      bool open = false;

      std::size_t        loops = 0;
      asio::steady_timer loops_event;

      asio::steady_timer heartbeat;

      std::deque<pb::byte_buffer> outbox;
      asio::steady_timer          outbox_event;

      std::string close_reason;
    };

    asio::awaitable<void>
    read_loop (std::shared_ptr<connection>);

    asio::awaitable<void>
    write_loop (std::shared_ptr<connection>);

    asio::awaitable<void>
    heartbeat_loop (std::shared_ptr<connection>);

    void
    spawn_loop (std::shared_ptr<connection> c, asio::awaitable<void> loop);

    asio::awaitable<void>
    join_loops (std::shared_ptr<connection> c);

    void
    dispatch (pb::byte_view packet);

    void
    dispatch_message (steam_message&&);

    void
    abandon (const std::string& reason);

    void
    shutdown (const std::string& reason);

    std::uint64_t
    next_job_id () noexcept;

    void
    enqueue (pb::byte_buffer);

    void
    stamp (steam_message&) const;

  private:
    asio::io_context& ioc_;
    steam_cm_traits   traits_;
    ssl::context      ssl_ctx_;

    std::shared_ptr<connection> conn_;

    std::optional<std::uint64_t> steam_id_;
    std::optional<std::int32_t>  session_id_;

    std::vector<std::uint32_t> licenses_;
    bool                       licenses_received_ = false;

    asio::steady_timer licenses_event_;

    std::optional<steam_message> logon_response_;
    asio::steady_timer           logon_event_;

    std::map<std::uint64_t, std::shared_ptr<pending_job>> jobs_;

    std::uint64_t next_job_ = 1;

    steam_cm_endpoint endpoint_;
  };
}
