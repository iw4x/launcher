// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
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

  // HTTP client settings.
  //
  // The request timeout applies to each write and read separately, which for
  // a download means each chunk of the body.
  //
  // The proxy URL has the <scheme>://[<user>[:<pass>]@]<host>:<port> form
  // where <scheme> is http or one of the socks variants. Note that we tunnel
  // HTTPS through an HTTP proxy with CONNECT. An empty URL means no proxy.
  //
  // An empty certificate file means the default verify paths.
  //
  // @@ Nothing reads user_agent or keep_alive (see http_request::normalize()
  //    for the User-Agent that we send).
  //
  struct http_client_traits
  {
    std::chrono::milliseconds connect_timeout {30000};
    std::chrono::milliseconds request_timeout {60000};
    std::uint8_t max_redirects    = 10;
    bool verify_ssl               = false;
    std::string ssl_cert_file;
    std::string user_agent        = "iw4x-launcher/1.1";
    bool follow_redirects         = true;
    bool keep_alive               = true;
    std::string proxy_url;
  };

  // Settings and TLS context that every request of a client shares.
  //
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

  // HTTP and HTTPS client.
  //
  // Every request makes a new connection, which we close once we have read
  // the response.
  //
  // The functions throw std::runtime_error if the proxy URL is invalid,
  // if the redirect limit is exceeded, or if the proxy fails, and
  // boost::system::system_error on network and TLS errors.
  //
  class http_client
  {
  public:
    // Called with the number of bytes downloaded and the total, which is 0
    // if unknown.
    //
    using progress_callback =
      std::function<void (std::uint64_t, std::uint64_t)>;

    explicit
    http_client (asio::io_context& ioc);
    http_client (asio::io_context& ioc, const http_client_traits& traits);

    http_client (const http_client&) = delete;
    http_client& operator = (const http_client&) = delete;

    // Perform the request, following redirects if enabled, and return the
    // response, whatever its status.
    //
    asio::awaitable<http_response>
    request (http_request req);

    asio::awaitable<http_response>
    get (std::string url);

    asio::awaitable<http_response>
    post (std::string url,
          std::string body,
          std::string content_type = "application/json");

    asio::awaitable<http_response>
    put (std::string url,
         std::string body,
         std::string content_type = "application/json");

    asio::awaitable<http_response>
    delete_ (std::string url);

    asio::awaitable<http_response>
    head (std::string url);

    // Download the URL into the target and return the size of the target.
    //
    // If resume_from is specified, request the remainder of the file
    // starting from this offset and append it to the target. Otherwise,
    // overwrite the target. A rate limit of 0 means no limit.
    //
    // Throw http_status_error if the status is neither 200 nor 206.
    //
    asio::awaitable<std::uint64_t>
    download (std::string url,
              std::filesystem::path target_path,
              progress_callback progress = nullptr,
              std::optional<std::uint64_t> resume_from = std::nullopt,
              std::uint64_t rate_limit_bytes_per_second = 0);

    http_session&
    session () noexcept
    {
      return session_;
    }

    const http_session&
    session () const noexcept
    {
      return session_;
    }

  private:
    asio::awaitable<http_response>
    request_impl (http_request req, std::uint8_t redirect_count = 0);

    asio::awaitable<std::uint64_t>
    download_impl (std::string url,
                   std::filesystem::path target_path,
                   progress_callback progress,
                   std::optional<std::uint64_t> resume_from,
                   std::uint64_t rate_limit_bytes_per_second,
                   std::uint8_t redirect_count);

    asio::awaitable<http_response>
    request_ssl (http_request req);

    asio::awaitable<http_response>
    request_tcp (http_request req);

  private:
    http_session session_;
  };
}
