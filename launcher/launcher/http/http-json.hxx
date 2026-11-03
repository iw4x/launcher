// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include <boost/asio.hpp>
#include <boost/json.hpp>

#include <launcher/http/http-client.hxx>
#include <launcher/http/http-request.hxx>
#include <launcher/http/http-response.hxx>
#include <launcher/http/http-types.hxx>

namespace launcher
{
  // Parse the response body as JSON. Throw std::runtime_error if there is
  // no body or it is not valid JSON.
  //
  boost::json::value
  parse_json (const http_response& response);

  // Return a request with the value serialized as its JSON body.
  //
  http_request
  make_json_request (http_method method,
                     const std::string& url,
                     const boost::json::value& json);

  // HTTP client for JSON APIs.
  //
  // The functions return the parsed response body and throw
  // std::runtime_error if the status is not 2xx or the body is not valid
  // JSON, in addition to what http_client throws.
  //
  class json_http_client
  {
  public:
    explicit
    json_http_client (boost::asio::io_context& ioc);

    json_http_client (boost::asio::io_context& ioc,
                      const http_client_traits& traits);

    json_http_client (const json_http_client&) = delete;
    json_http_client& operator = (const json_http_client&) = delete;

    boost::asio::awaitable<boost::json::value>
    get_json (const std::string& url);

    boost::asio::awaitable<boost::json::value>
    post_json (const std::string& url, const boost::json::value& json);

    boost::asio::awaitable<boost::json::value>
    put_json (const std::string& url, const boost::json::value& json);

    boost::asio::awaitable<boost::json::value>
    patch_json (const std::string& url, const boost::json::value& json);

    // Return a null value if the response has no body.
    //
    boost::asio::awaitable<boost::json::value>
    delete_json (const std::string& url);

    http_client&
    client () noexcept
    {
      return client_;
    }

    const http_client&
    client () const noexcept
    {
      return client_;
    }

  private:
    http_client client_;
  };
}
