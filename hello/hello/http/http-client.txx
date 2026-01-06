#include <fstream>
#include <stdexcept>
#include <iostream>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <openssl/ssl.h>

namespace hello
{
  namespace http = beast::http;
  using tcp = asio::ip::tcp;

  struct url_parts
  {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target;
  };

  // URL parser.
  //
  inline url_parts
  parse_url (const std::string& url)
  {
    url_parts parts;
    std::size_t pos (0);

    // Parse scheme.
    //
    std::size_t scheme_end (url.find ("://", pos));
    if (scheme_end != std::string::npos)
    {
      parts.scheme = url.substr (0, scheme_end);
      pos = scheme_end + 3;
    }
    else
    {
      parts.scheme = "http";
    }

    // Parse authority (host:port).
    //
    std::size_t auth_end (url.find ('/', pos));
    if (auth_end == std::string::npos)
      auth_end = url.size ();

    std::string authority (url.substr (pos, auth_end - pos));
    std::size_t colon_pos (authority.find (':'));

    if (colon_pos != std::string::npos)
    {
      parts.host = authority.substr (0, colon_pos);
      parts.port = authority.substr (colon_pos + 1);
    }
    else
    {
      parts.host = authority;
      parts.port = (parts.scheme == "https") ? "443" : "80";
    }

    // Parse target (path + query + fragment).
    //
    if (auth_end < url.size ())
      parts.target = url.substr (auth_end);
    else
      parts.target = "/";

    return parts;
  }

  // Helper: Convert http_method to Beast verb.
  //
  inline http::verb
  to_beast_verb (http_method method)
  {
    switch (method)
    {
      case http_method::get:     return http::verb::get;
      case http_method::head:    return http::verb::head;
      case http_method::post:    return http::verb::post;
      case http_method::put:     return http::verb::put;
      case http_method::delete_: return http::verb::delete_;
      case http_method::connect: return http::verb::connect;
      case http_method::options: return http::verb::options;
      case http_method::trace:   return http::verb::trace;
      case http_method::patch:   return http::verb::patch;
    }
    return http::verb::get;
  }

  // Helper: Convert Beast status to http_status.
  //
  inline http_status
  from_beast_status (http::status s)
  {
    return static_cast<http_status> (static_cast<std::uint16_t> (s));
  }

  // basic_http_client template implementations.
  //
  template <typename T>
  asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  request_impl (request_type req, std::uint8_t redirect_count)
  {
    const auto& traits (session_->traits ());

    if (redirect_count >= traits.max_redirects)
      throw std::runtime_error ("maximum redirects exceeded");

    url_parts parts (parse_url (req.url));
    bool use_ssl (parts.scheme == "https");

    response_type res (use_ssl ? co_await request_ssl (req)
                               : co_await request_tcp (req));

    // Handle redirects.
    //
    if (traits.follow_redirects && res.is_redirection ())
    {
      auto location (res.location ());

      if (location)
      {
        // Make a new request with the redirect URL.
        //
        request_type new_req (req.method, *location, req.version);
        new_req.headers = req.headers;

        // For 303 See Other, change POST/PUT to GET.
        //
        if (res.status == http_status::see_other)
        {
          if (req.method == http_method::post ||
              req.method == http_method::put)
          {
            new_req.method = http_method::get;
            new_req.body = std::nullopt;
          }
        }

        new_req.normalize ();

        co_return co_await request_impl (
          std::move (new_req), redirect_count + 1);
      }
    }

    co_return res;
  }

  template <typename T>
  asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  request_ssl (const request_type& req)
  {
    url_parts parts (parse_url (req.url));

    tcp::resolver resolver (session_->io_context ());
    auto const results (co_await resolver.async_resolve (
      parts.host, parts.port, asio::use_awaitable));

    beast::ssl_stream<beast::tcp_stream> stream (
      session_->io_context (),
      session_->ssl_context ());

    if (!SSL_set_tlsext_host_name (stream.native_handle (), parts.host.c_str ()))
      throw beast::system_error (
        beast::error_code (
          static_cast<int> (::ERR_get_error ()),
          asio::error::get_ssl_category ()),
        "Failed to set SNI hostname");

    beast::get_lowest_layer (stream).expires_after (
      std::chrono::milliseconds (session_->traits ().connect_timeout));

    co_await beast::get_lowest_layer (stream).async_connect (
      results, asio::use_awaitable);

    co_await stream.async_handshake (
      ssl::stream_base::client, asio::use_awaitable);

    http::request<http::string_body> beast_req;
    beast_req.method (to_beast_verb (req.method));
    beast_req.target (req.target ());
    beast_req.version (req.version.major * 10 + req.version.minor);

    for (const auto& field : req.headers)
      beast_req.set (field.name, field.value);

    if (req.body)
    {
      beast_req.body () = *req.body;
      beast_req.prepare_payload ();
    }

    beast::get_lowest_layer (stream).expires_after (
      std::chrono::milliseconds (session_->traits ().request_timeout));

    co_await http::async_write (stream, beast_req, asio::use_awaitable);

    beast::flat_buffer buffer;
    http::response<http::string_body> beast_res;
    co_await http::async_read (stream, buffer, beast_res, asio::use_awaitable);

    beast::error_code ec;
    co_await stream.async_shutdown (asio::redirect_error (asio::use_awaitable, ec));

    response_type res;
    res.status = from_beast_status (beast_res.result ());
    res.version = http_version (
      beast_res.version () / 10,
      beast_res.version () % 10);
    res.reason = string_type (beast_res.reason ());

    for (const auto& field : beast_res)
      res.headers.add (
        string_type (field.name_string ()),
        string_type (field.value ()));

    if (!beast_res.body ().empty ())
      res.body = beast_res.body ();

    co_return res;
  }

  template <typename T>
  asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  request_tcp (const request_type& req)
  {
    url_parts parts (parse_url (req.url));

    tcp::resolver resolver (session_->io_context ());
    auto const results (co_await resolver.async_resolve (
      parts.host, parts.port, asio::use_awaitable));

    beast::tcp_stream stream (session_->io_context ());

    stream.expires_after (
      std::chrono::milliseconds (session_->traits ().connect_timeout));

    co_await stream.async_connect (results, asio::use_awaitable);

    http::request<http::string_body> beast_req;
    beast_req.method (to_beast_verb (req.method));
    beast_req.target (parts.target);
    beast_req.version (req.version.major * 10 + req.version.minor);

    for (const auto& field : req.headers)
      beast_req.set (field.name, field.value);

    if (req.body)
    {
      beast_req.body () = *req.body;
      beast_req.prepare_payload ();
    }

    stream.expires_after (
      std::chrono::milliseconds (session_->traits ().request_timeout));

    co_await http::async_write (stream, beast_req, asio::use_awaitable);

    beast::flat_buffer buffer;
    http::response<http::string_body> beast_res;
    co_await http::async_read (stream, buffer, beast_res, asio::use_awaitable);

    beast::error_code ec;
    stream.socket ().shutdown (tcp::socket::shutdown_both, ec);

    response_type res;
    res.status = from_beast_status (beast_res.result ());
    res.version = http_version (
      beast_res.version () / 10,
      beast_res.version () % 10);
    res.reason = string_type (beast_res.reason ());

    for (const auto& field : beast_res)
      res.headers.add (
        string_type (field.name_string ()),
        string_type (field.value ()));

    if (!beast_res.body ().empty ())
      res.body = beast_res.body ();

    co_return res;
  }

  template <typename T>
  asio::awaitable<std::uint64_t>
  basic_http_client<T>::
  download (const string_type& url,
            const string_type& target_path,
            progress_callback progress,
            std::optional<std::uint64_t> resume_from)
  {
    co_return co_await download_impl (url, target_path, progress, resume_from, 0);
  }

  // Internal download implementation with redirect handling.
  //
  template <typename T>
  asio::awaitable<std::uint64_t>
  basic_http_client<T>::
  download_impl (const string_type& url,
                 const string_type& target_path,
                 progress_callback progress,
                 std::optional<std::uint64_t> resume_from,
                 std::uint8_t redirect_count)
  {
    const auto& traits (session_->traits ());

    if (redirect_count >= traits.max_redirects)
      throw std::runtime_error ("maximum redirects exceeded");

    request_type req (http_method::get, url);

    if (resume_from)
      req.set_header (
        string_type ("Range"),
        string_type ("bytes=") + std::to_string (*resume_from) + string_type ("-"));

    req.normalize ();

    url_parts parts (parse_url (url));
    bool use_ssl (parts.scheme == "https");

    tcp::resolver resolver (session_->io_context ());
    auto const results (co_await resolver.async_resolve (
      parts.host, parts.port, asio::use_awaitable));

    std::ios_base::openmode mode (std::ios::binary | std::ios::out);
    if (resume_from)
      mode |= std::ios::app;
    else
      mode |= std::ios::trunc;

    std::ofstream ofs (target_path, mode);
    if (!ofs)
      throw std::runtime_error ("failed to open file for writing");

    std::uint64_t bytes_transferred (resume_from ? *resume_from : 0);
    std::uint64_t total_bytes (0);

    if (use_ssl)
    {
      beast::ssl_stream<beast::tcp_stream> stream (
        session_->io_context (),
        session_->ssl_context ());

      if (!SSL_set_tlsext_host_name (stream.native_handle (), parts.host.c_str ()))
        throw beast::system_error (
          beast::error_code (
            static_cast<int> (::ERR_get_error ()),
            asio::error::get_ssl_category ()),
          "Failed to set SNI hostname");

      beast::get_lowest_layer (stream).expires_after (
        std::chrono::milliseconds (session_->traits ().connect_timeout));

      co_await beast::get_lowest_layer (stream).async_connect (
        results, asio::use_awaitable);

      co_await stream.async_handshake (
        ssl::stream_base::client, asio::use_awaitable);

      // Build and send request.
      //
      http::request<http::empty_body> beast_req;
      beast_req.method (http::verb::get);
      beast_req.target (parts.target);
      beast_req.version (11);
      beast_req.set (http::field::host, parts.host);

      for (const auto& field : req.headers)
        beast_req.set (field.name, field.value);

      beast::get_lowest_layer (stream).expires_after (
        std::chrono::milliseconds (session_->traits ().request_timeout));

      co_await http::async_write (stream, beast_req, asio::use_awaitable);

      beast::flat_buffer buffer;
      http::response_parser<http::buffer_body> parser;
      parser.body_limit (std::numeric_limits<std::uint64_t>::max ());

      co_await http::async_read_header (
        stream, buffer, parser, asio::use_awaitable);

      auto status = parser.get().result_int();

      if (traits.follow_redirects && status >= 300 && status < 400)
      {
        auto location = parser.get()[http::field::location];
        if (!location.empty())
        {
          ofs.close();

          beast::error_code ec;
          stream.shutdown (ec);

          co_return co_await download_impl (
            string_type(location), target_path, progress, resume_from, redirect_count + 1);
        }
      }

      if (status != 200 && status != 206)
        throw std::runtime_error (
          "download failed with status: " + std::to_string(status));

      if (parser.content_length ())
        total_bytes = *parser.content_length () + bytes_transferred;

      char download_buffer[8192];
      parser.get ().body ().data = download_buffer;
      parser.get ().body ().size = sizeof (download_buffer);

      std::size_t total_written (0);

      while (!parser.is_done ())
      {
        std::size_t bytes_read = co_await http::async_read_some (
          stream, buffer, parser, asio::use_awaitable);

        if (bytes_read > 0)
        {
          ofs.write (download_buffer, bytes_read);
          total_written += bytes_read;
          bytes_transferred += bytes_read;

          if (progress)
            progress (bytes_transferred, total_bytes);

          parser.get ().body ().data = download_buffer;
          parser.get ().body ().size = sizeof (download_buffer);
        }
      }

      ofs.flush ();
    }
    else
    {
      beast::tcp_stream stream (session_->io_context ());

      stream.expires_after (
        std::chrono::milliseconds (session_->traits ().connect_timeout));

      co_await stream.async_connect (results, asio::use_awaitable);

      // Build and send request.
      //
      http::request<http::empty_body> beast_req;
      beast_req.method (http::verb::get);
      beast_req.target (parts.target);
      beast_req.version (11);
      beast_req.set (http::field::host, parts.host);

      for (const auto& field : req.headers)
        beast_req.set (field.name, field.value);

      stream.expires_after (
        std::chrono::milliseconds (session_->traits ().request_timeout));

      co_await http::async_write (stream, beast_req, asio::use_awaitable);

      beast::flat_buffer buffer;
      http::response_parser<http::buffer_body> parser;
      parser.body_limit (std::numeric_limits<std::uint64_t>::max ());

      co_await http::async_read_header (
        stream, buffer, parser, asio::use_awaitable);

      auto status = parser.get().result_int();

      if (traits.follow_redirects && status >= 300 && status < 400)
      {
        auto location = parser.get()[http::field::location];
        if (!location.empty())
        {
          beast::error_code ec;
          stream.socket ().shutdown (tcp::socket::shutdown_both, ec);

          ofs.close();

          // Follow redirect.
          //
          co_return co_await download_impl (
            string_type(location), target_path, progress, resume_from, redirect_count + 1);
        }
      }

      // Check for success status.
      //
      if (status != 200 && status != 206)
        throw std::runtime_error (
          "download failed with status: " + std::to_string(status));

      // Get content length if available.
      //
      if (parser.content_length ())
        total_bytes = *parser.content_length () + bytes_transferred;

      char download_buffer[8192];
      parser.get ().body ().data = download_buffer;
      parser.get ().body ().size = sizeof (download_buffer);

      std::size_t total_written (0);

      while (!parser.is_done ())
      {
        std::size_t bytes_read = co_await http::async_read_some (
          stream, buffer, parser, asio::use_awaitable);

        if (bytes_read > 0)
        {
          ofs.write (download_buffer, bytes_read);
          total_written += bytes_read;
          bytes_transferred += bytes_read;

          if (progress)
            progress (bytes_transferred, total_bytes);

          parser.get ().body ().data = download_buffer;
          parser.get ().body ().size = sizeof (download_buffer);
        }
      }

      // Flush the file before shutdown.
      //
      ofs.flush ();

      beast::error_code ec;
      stream.socket ().shutdown (tcp::socket::shutdown_both, ec);
    }

    // Ensure file is flushed and closed properly.
    //
    ofs.flush ();
    ofs.close ();

    if (!ofs)
      throw std::runtime_error ("failed to write file");

    co_return bytes_transferred;
  }
}
