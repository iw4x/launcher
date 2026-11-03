// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <launcher/http/http.hxx>

#include <boost/asio.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace launcher
{
  namespace fs = std::filesystem;
  namespace asio = boost::asio;

  // HTTP requests that treat an error status as a failure.
  //
  class http_coordinator
  {
  public:
    using client_type = http_client;
    using request_type = http_request;
    using response_type = http_response;

    // Called as a download progresses, with the number of bytes received so
    // far and the total, which is 0 if the server doesn't report it.
    //
    using progress_callback =
      std::function<void (std::uint64_t bytes_transferred,
                          std::uint64_t total_bytes)>;

    explicit
    http_coordinator (asio::io_context& ioc);
    http_coordinator (asio::io_context& ioc, const http_client_traits& traits);

    http_coordinator (const http_coordinator&) = delete;
    http_coordinator& operator = (const http_coordinator&) = delete;

    // Issue the GET request and return the response body, which is empty if
    // the response has none. Throw std::runtime_error if the response has an
    // error status and the client's exceptions if the request fails.
    //
    asio::awaitable<std::string>
    get (const std::string& url);

    // As above but return the whole response.
    //
    asio::awaitable<response_type>
    get_response (const std::string& url);

    // Issue the POST request with the JSON body and return the response body
    // the same way as get().
    //
    asio::awaitable<std::string>
    post_json (const std::string& url, const std::string& json);

    // Download the URL into the file, creating its directory if necessary,
    // and return the number of bytes received. If resume_from is specified,
    // then request the content from this offset, which only works if the
    // server supports range requests.
    //
    asio::awaitable<std::uint64_t>
    download_file (const std::string& url,
                   const fs::path& target,
                   progress_callback progress = nullptr,
                   std::optional<std::uint64_t> resume_from = std::nullopt);

    // Issue the HEAD request and return the content length, if the server
    // reports it. Throw the same way as get().
    //
    asio::awaitable<std::optional<std::uint64_t>>
    get_content_length (const std::string& url);

    // Return true if the HEAD request gets a 2xx status and false otherwise,
    // including if the request fails.
    //
    asio::awaitable<bool>
    check_url (const std::string& url);

    client_type&
    client () noexcept;

    const client_type&
    client () const noexcept;

  private:
    client_type client_;
  };
}
