#pragma once

#include <string>
#include <memory>
#include <functional>
#include <cstdint>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

#include <optional>

#include <hello/http/http-types.hxx>
#include <hello/http/http-request.hxx>
#include <hello/http/http-response.hxx>

namespace hello
{
  namespace asio  = boost::asio;
  namespace beast = boost::beast;
  namespace ssl   = boost::asio::ssl;

  // HTTP client options/configuration traits.
  //
  template <typename S = std::string>
  struct http_client_traits
  {
    using string_type   = S;
    using request_type  = basic_http_request<string_type>;
    using response_type = basic_http_response<string_type>;

    // Connection timeout in milliseconds (0 = no timeout).
    //
    std::uint32_t connect_timeout = 30000;

    // Request timeout in milliseconds (0 = no timeout).
    //
    std::uint32_t request_timeout = 60000;

    // Maximum number of redirects to follow (0 = no redirects).
    //
    std::uint8_t max_redirects = 10;

    // Whether to verify SSL certificates.
    //
    bool verify_ssl = false;

    // SSL certificate file path (empty = use system defaults).
    //
    string_type ssl_cert_file;

    // Default user agent.
    //
    string_type user_agent = string_type ("iw4x-launcher/1.1");

    // Whether to automatically follow redirects.
    //
    bool follow_redirects = true;

    // Whether to keep connections alive.
    //
    bool keep_alive = true;
  };

  // HTTP client session context.
  //
  // Manages connection state and reuse for a specific host.
  //
  template <typename T = http_client_traits<>>
  class basic_http_session
  {
  public:
    using traits_type = T;
    using string_type = typename traits_type::string_type;

    explicit
    basic_http_session (asio::io_context& ioc, const traits_type& traits)
      : ioc_ (ioc), traits_ (traits), ssl_ctx_ (ssl::context::tlsv12_client)
    {
      configure_ssl ();
    }

    basic_http_session (const basic_http_session&) = delete;
    basic_http_session& operator= (const basic_http_session&) = delete;

    asio::io_context&
    io_context () noexcept
    {
      return ioc_;
    }

    const traits_type&
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
    traits_type traits_;
    ssl::context ssl_ctx_;
  };

  // HTTP client.
  //
  // Provides high-level async HTTP operations using Boost.Beast and
  // coroutines.
  //
  template <typename T = http_client_traits<>>
  class basic_http_client
  {
  public:
    using traits_type   = T;
    using string_type   = typename traits_type::string_type;
    using request_type  = typename traits_type::request_type;
    using response_type = typename traits_type::response_type;
    using session_type  = basic_http_session<traits_type>;

    // Progress callback: (bytes_transferred, total_bytes).
    // total_bytes may be 0 if unknown.
    //
    using progress_callback = std::function<void(std::uint64_t, std::uint64_t)>;

    // Constructors.
    //
    explicit
    basic_http_client (asio::io_context& ioc)
      : session_ (std::make_unique<session_type> (ioc, traits_type ())) {}

    basic_http_client (asio::io_context& ioc, const traits_type& traits)
      : session_ (std::make_unique<session_type> (ioc, traits)) {}

    basic_http_client (const basic_http_client&) = delete;
    basic_http_client& operator= (const basic_http_client&) = delete;

    // Perform an HTTP request and return the response.
    //
    asio::awaitable<response_type>
    request (const request_type& req);

    // Perform a GET request.
    //
    asio::awaitable<response_type>
    get (const string_type& url);

    // Perform a POST request.
    //
    asio::awaitable<response_type>
    post (const string_type& url,
          const string_type& body,
          const string_type& content_type = string_type ("application/json"));

    // Perform a PUT request.
    //
    asio::awaitable<response_type>
    put (const string_type& url,
         const string_type& body,
         const string_type& content_type = string_type ("application/json"));

    // Perform a DELETE request.
    //
    asio::awaitable<response_type>
    delete_ (const string_type& url);

    // Perform a HEAD request.
    //
    asio::awaitable<response_type>
    head (const string_type& url);

    // Download a file with progress tracking and optional resume support.
    //
    // Returns the number of bytes downloaded.
    //
    asio::awaitable<std::uint64_t>
    download (const string_type& url,
              const string_type& target_path,
              progress_callback progress = nullptr,
              std::optional<std::uint64_t> resume_from = std::nullopt);

    // Get the session.
    //
    session_type&
    session () noexcept
    {
      return *session_;
    }

    const session_type&
    session () const noexcept
    {
      return *session_;
    }

  private:
    // Internal request implementation with redirect handling.
    //
    asio::awaitable<response_type>
    request_impl (request_type req, std::uint8_t redirect_count = 0);

    // Internal download implementation with redirect handling.
    //
    asio::awaitable<std::uint64_t>
    download_impl (const string_type& url,
                   const string_type& target_path,
                   progress_callback progress,
                   std::optional<std::uint64_t> resume_from,
                   std::uint8_t redirect_count);

    // Perform request over SSL.
    //
    asio::awaitable<response_type>
    request_ssl (const request_type& req);

    // Perform request over plain TCP.
    //
    asio::awaitable<response_type>
    request_tcp (const request_type& req);

  private:
    std::unique_ptr<session_type> session_;
  };

  // Common typedefs.
  //
  using http_session = basic_http_session<>;
  using http_client  = basic_http_client<>;
}

#include <hello/http/http-client.ixx>
#include <hello/http/http-client.txx>
