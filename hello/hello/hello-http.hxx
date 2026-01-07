#pragma once

#include <hello/http/http.hxx>

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <filesystem>
#include <optional>
#include <functional>

namespace hello
{
  namespace fs = std::filesystem;
  namespace asio = boost::asio;

  class http_coordinator
  {
  public:
    using client_type = http_client;
    using request_type = http_request;
    using response_type = http_response;

    // Progress callback for file downloads.
    //
    using progress_callback =
      std::function<void (std::uint64_t bytes_transferred,
                          std::uint64_t total_bytes)>;

    // Constructors.
    //
    explicit
    http_coordinator (asio::io_context& ioc);

    http_coordinator (asio::io_context& ioc,
                      const http_client_traits<>& traits);

    http_coordinator (const http_coordinator&) = delete;
    http_coordinator& operator= (const http_coordinator&) = delete;

    // GET request returning body as string.
    //
    // Throws on HTTP error or network failure.
    //
    asio::awaitable<std::string>
    get (const std::string& url);

    // GET request returning full response.
    //
    asio::awaitable<response_type>
    get_response (const std::string& url);

    // POST request with JSON body.
    //
    asio::awaitable<std::string>
    post_json (const std::string& url, const std::string& json);

    // Download file to specified path.
    //
    // Returns the number of bytes downloaded.
    //
    // If resume_from is specified, attempts to resume download from that
    // byte offset (requires server support via Range header).
    //
    asio::awaitable<std::uint64_t>
    download_file (const std::string& url,
                   const fs::path& target,
                   progress_callback progress = nullptr,
                   std::optional<std::uint64_t> resume_from = std::nullopt);

    // HEAD request to get file size without downloading.
    //
    // Returns content length if available.
    //
    asio::awaitable<std::optional<std::uint64_t>>
    get_content_length (const std::string& url);

    // Check if a URL is accessible.
    //
    // Returns true if the server responds with 2xx status.
    //
    asio::awaitable<bool>
    check_url (const std::string& url);

    // Access underlying client.
    //
    client_type&
    client () noexcept;

    const client_type&
    client () const noexcept;

  private:
    asio::io_context& ioc_;
    std::unique_ptr<client_type> client_;
  };

  // Parse JSON response body.
  //
  // Convenience function for parsing JSON from HTTP response bodies.
  // Throws if JSON is malformed.
  //
  boost::json::value
  parse_json (const std::string& body);

  // Format HTTP error message.
  //
  // Creates a descriptive error message from an HTTP response.
  //
  std::string
  format_http_error (const http_response& response);
}
