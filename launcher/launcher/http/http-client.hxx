#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

#include <optional>

#include <launcher/http/http-request.hxx>
#include <launcher/http/http-response.hxx>
#include <launcher/http/http-types.hxx>

namespace launcher
{
  namespace asio = boost::asio;
  namespace beast = boost::beast;
  namespace ssl = boost::asio::ssl;

  struct http_client_traits
  {
    std::uint32_t connect_timeout = 30000;
    std::uint32_t request_timeout = 60000;
    std::uint8_t max_redirects    = 10;
    bool verify_ssl               = false;
    std::string ssl_cert_file;
    std::string user_agent        = "iw4x-launcher/1.1";
    bool follow_redirects         = true;
    bool keep_alive               = true;
  };

  class http_session
  {
  public:
    explicit
    http_session (asio::io_context& ioc,
                  const http_client_traits& traits);

    http_session (const http_session&) = delete;
    http_session& operator = (const http_session&) = delete;

    asio::io_context&
    io_context () noexcept
    {
      return ioc_;
    }

    const http_client_traits&
    traits () const noexcept
    {
      return traits_;
    }

    ssl::context&
    ssl_context () noexcept
    {
      return ssl_ctx_;
    }

  private:
    void
    configure_ssl ();

  private:
    asio::io_context& ioc_;
    http_client_traits traits_;
    ssl::context ssl_ctx_;
  };

  class http_client
  {
  public:
    using progress_callback =
      std::function<void (std::uint64_t, std::uint64_t)>;

    explicit
    http_client (asio::io_context& ioc);
    http_client (asio::io_context& ioc, const http_client_traits& traits);

    http_client (const http_client&) = delete;
    http_client& operator = (const http_client&) = delete;

    asio::awaitable<http_response>
    request (const http_request& req);

    asio::awaitable<http_response>
    get (const std::string& url);

    asio::awaitable<http_response>
    post (const std::string& url,
          const std::string& body,
          const std::string& content_type = "application/json");

    asio::awaitable<http_response>
    put (const std::string& url,
         const std::string& body,
         const std::string& content_type = "application/json");

    asio::awaitable<http_response>
    delete_ (const std::string& url);

    asio::awaitable<http_response>
    head (const std::string& url);

    asio::awaitable<std::uint64_t>
    download (const std::string& url,
              const std::string& target_path,
              progress_callback progress = nullptr,
              std::optional<std::uint64_t> resume_from = std::nullopt,
              std::uint64_t rate_limit_bytes_per_second = 0);

    http_session&
    session () noexcept
    {
      return *session_;
    }

    const http_session&
    session () const noexcept
    {
      return *session_;
    }

  private:
    asio::awaitable<http_response>
    request_impl (http_request req, std::uint8_t redirect_count = 0);

    asio::awaitable<std::uint64_t>
    download_impl (const std::string& url,
                   const std::string& target_path,
                   progress_callback progress,
                   std::optional<std::uint64_t> resume_from,
                   std::uint64_t rate_limit_bytes_per_second,
                   std::uint8_t redirect_count);

    asio::awaitable<http_response>
    request_ssl (const http_request& req);

    asio::awaitable<http_response>
    request_tcp (const http_request& req);

  private:
    std::unique_ptr<http_session> session_;
  };
}
