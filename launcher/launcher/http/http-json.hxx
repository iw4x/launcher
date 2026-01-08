#pragma once

#include <string>

#include <boost/json.hpp>
#include <boost/asio.hpp>

#include <launcher/http/http-types.hxx>
#include <launcher/http/http-request.hxx>
#include <launcher/http/http-response.hxx>
#include <launcher/http/http-client.hxx>

namespace launcher
{
  // JSON HTTP request/response helpers.
  //

  // Parse JSON from HTTP response body.
  //
  template <typename S>
  boost::json::value
  parse_json (const basic_http_response<S>& response);

  // Create JSON request body from Boost.JSON value.
  //
  template <typename S>
  basic_http_request<S>
  make_json_request (http_method method,
                     const S& url,
                     const boost::json::value& json);

  // JSON HTTP client wrapper.
  //
  template <typename T = http_client_traits<>>
  class basic_json_http_client
  {
  public:
    using traits_type   = T;
    using string_type   = typename traits_type::string_type;
    using client_type   = basic_http_client<traits_type>;
    using request_type  = typename traits_type::request_type;
    using response_type = typename traits_type::response_type;

    // Constructors.
    //
    explicit
    basic_json_http_client (boost::asio::io_context& ioc)
      : client_ (ioc) {}

    basic_json_http_client (boost::asio::io_context& ioc,
                            const traits_type& traits)
      : client_ (ioc, traits) {}

    basic_json_http_client (const basic_json_http_client&) = delete;
    basic_json_http_client& operator= (const basic_json_http_client&) = delete;

    // Perform JSON GET request.
    //
    boost::asio::awaitable<boost::json::value>
    get_json (const string_type& url);

    // Perform JSON POST request.
    //
    boost::asio::awaitable<boost::json::value>
    post_json (const string_type& url,
               const boost::json::value& json);

    // Perform JSON PUT request.
    //
    boost::asio::awaitable<boost::json::value>
    put_json (const string_type& url,
              const boost::json::value& json);

    // Perform JSON PATCH request.
    //
    boost::asio::awaitable<boost::json::value>
    patch_json (const string_type& url,
                const boost::json::value& json);

    // Perform JSON DELETE request (may return JSON response).
    //
    boost::asio::awaitable<boost::json::value>
    delete_json (const string_type& url);

    // Get underlying HTTP client.
    //
    client_type&
    client () noexcept
    {
      return client_;
    }

    const client_type&
    client () const noexcept
    {
      return client_;
    }

  private:
    client_type client_;
  };

  // Common typedefs.
  //
  using json_http_client = basic_json_http_client<>;
}

#include <launcher/http/http-json.ixx>
