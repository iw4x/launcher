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

  // URL parts structure.
  //
  struct url_parts
  {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target;
  };

  // Parse a simple URL string into its components.
  //
  // Note that we are doing this manually here to avoid introducing a dependency
  // on a full-blown URI library. This handles the standard
  // scheme://host:port/path format but might struggle with more exotic
  // constructions (e.g., IPv6 literals or user info).
  //
  // If we ever need to support those, we should probably bite the bullet and
  // pull in a proper parser.
  //
  inline url_parts
  parse_url (const std::string& url)
  {
    url_parts r;
    std::size_t pos (0);

    // Parse scheme.
    //
    std::size_t p (url.find ("://", pos));
    if (p != std::string::npos)
    {
      r.scheme = url.substr (0, p);
      pos = p + 3;
    }
    else
      // Fallback to http if no scheme is specified.
      //
      r.scheme = "http";

    // Parse authority (host:port).
    //
    // We assume the authority ends at the first slash (start of the path) or
    // the end of the string.
    //
    std::size_t end (url.find ('/', pos));
    if (end == std::string::npos)
      end = url.size ();

    std::string auth (url.substr (pos, end - pos));
    std::size_t colon (auth.find (':'));

    if (colon != std::string::npos)
    {
      r.host = auth.substr (0, colon);
      r.port = auth.substr (colon + 1);
    }
    else
    {
      r.host = auth;
      r.port = (r.scheme == "https") ? "443" : "80";
    }

    // Parse target (path + query + fragment).
    //
    if (end < url.size ())
      r.target = url.substr (end);
    else
      r.target = "/";

    return r;
  }

  // Helper: Convert our internal http_method enum to Beast verb.
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

  // Helper: Convert Beast status to our internal http_status.
  //
  inline http_status
  from_beast_status (http::status s)
  {
    return static_cast<http_status> (static_cast<std::uint16_t> (s));
  }

  // Execute a request with automatic redirect handling.
  //
  template <typename T>
  asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  request_impl (request_type req, std::uint8_t redirect_count)
  {
    const auto& traits (session_->traits ());

    if (redirect_count >= traits.max_redirects)
      throw std::runtime_error ("maximum redirects exceeded");

    // Parse the URL and decide on the protocol.
    //
    url_parts parts (parse_url (req.url));
    bool ssl (parts.scheme == "https");

    response_type r (ssl
                     ? co_await request_ssl (req)
                     : co_await request_tcp (req));

    // Handle redirects (3xx).
    //
    // If the server told us to go elsewhere and the user allowed it, we
    // recurse.
    //
    if (traits.follow_redirects && r.is_redirection ())
    {
      auto loc (r.location ());

      if (loc)
      {
        // Construct the new request. We generally keep the method and headers
        // identical, with a specific exception for '303 See Other'.
        //
        request_type next_req (req.method, *loc, req.version);
        next_req.headers = req.headers;

        // For '303 See Other', RFC 7231 says we must change the method to GET
        // and drop the body.
        //
        if (r.status == http_status::see_other)
        {
          if (req.method == http_method::post ||
              req.method == http_method::put)
          {
            next_req.method = http_method::get;
            next_req.body = std::nullopt;
          }
        }

        next_req.normalize ();

        co_return co_await request_impl (std::move (next_req),
                                         redirect_count + 1);
      }
    }

    co_return r;
  }

  // Request implementation (SSL).
  //
  template <typename T>
  asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  request_ssl (const request_type& req)
  {
    using stream_type = beast::ssl_stream<beast::tcp_stream>;
    using beast_req   = http::request<http::string_body>;
    using beast_res   = http::response<http::string_body>;

    // Grab references to the session context to keep the code flat.
    //
    auto& ctx (session_->io_context ());
    auto& ssl (session_->ssl_context ());
    const auto& tr (session_->traits ());

    url_parts parts (parse_url (req.url));

    // Resolve the hostname.
    //
    tcp::resolver rslv (ctx);
    auto addrs (co_await rslv.async_resolve (
      parts.host, parts.port, asio::use_awaitable));

    stream_type s (ctx, ssl);

    // Set the SNI (Server Name Indication) hostname.
    //
    // Note that since Beast doesn't wrap this functionality directly (it's a
    // lower-level TLS feature), we have to drop down to the OpenSSL C API using
    // the native handle.
    //
    // Note also that If we fail here, the handshake will almost certainly fail
    // or return the wrong certificate later, so we treat it as a system error.
    //
    if (!SSL_set_tlsext_host_name (s.native_handle (), parts.host.c_str ()))
    {
      int v (static_cast<int> (::ERR_get_error ()));
      beast::error_code ec (v, asio::error::get_ssl_category ());

      throw beast::system_error (ec, "Failed to set SNI hostname");
    }

    // Connect to the TCP endpoint.
    //
    // We cache the reference to the lowest layer (the TCP stream) here. This
    // allows us to set the timeout on the underlying socket without verbose
    // casting calls cluttering the logic.
    //
    auto& layer (beast::get_lowest_layer (s));
    layer.expires_after (std::chrono::milliseconds (tr.connect_timeout));

    co_await layer.async_connect (addrs, asio::use_awaitable);

    // Perform the SSL handshake.
    //
    co_await s.async_handshake (
      ssl::stream_base::client, asio::use_awaitable);

    // Prepare the Beast request object.
    //
    beast_req br;
    br.method (to_beast_verb (req.method));
    br.target (req.target ());
    br.version (req.version.major * 10 + req.version.minor);

    for (const auto& h : req.headers)
      br.set (h.name, h.value);

    if (req.body)
    {
      br.body () = *req.body;
      br.prepare_payload ();
    }

    // Send the request.
    //
    // We reset the timeout on the lowest layer to the request timeout value
    // before writing.
    //
    layer.expires_after (std::chrono::milliseconds (tr.request_timeout));
    co_await http::async_write (s, br, asio::use_awaitable);

    // Receive the response.
    //
    beast::flat_buffer b;
    beast_res bres;
    co_await http::async_read (s, b, bres, asio::use_awaitable);

    // Shutdown.
    //
    // We attempt to shut down the SSL layer, but note that explicitly ignore
    // the error code here. The reality is that many servers simply close the
    // TCP connection after sending the response without performing a proper TLS
    // shutdown sequence. Treating that as a failure would cause us to throw
    // errors on perfectly valid requests.
    //
    beast::error_code ec;
    co_await s.async_shutdown (
      asio::redirect_error (asio::use_awaitable, ec));

    // Convert to our internal response type.
    //
    response_type r;
    r.status  = from_beast_status (bres.result  ());
    r.version = http_version      (bres.version () / 10,
                                   bres.version () % 10);
    r.reason  = string_type       (bres.reason  ());

    for (const auto& h : bres)
      r.headers.add (string_type (h.name_string ()),
                     string_type (h.value ()));

    if (!bres.body ().empty ())
      r.body = bres.body ();

    co_return r;
  }

  // Request implementation (TCP).
  //
  // This is the unencrypted counterpart to request_ssl. The logic is nearly
  // identical, minus the SNI setup and handshake steps.
  //
  template <typename T>
  asio::awaitable<typename basic_http_client<T>::response_type>
  basic_http_client<T>::
  request_tcp (const request_type& req)
  {
    using beast_req = http::request<http::string_body>;
    using beast_res = http::response<http::string_body>;

    auto& ctx (session_->io_context ());
    const auto& tr (session_->traits ());

    url_parts parts (parse_url (req.url));

    // Resolve.
    //
    tcp::resolver rslv (ctx);
    auto addrs (co_await rslv.async_resolve (
      parts.host, parts.port, asio::use_awaitable));

    beast::tcp_stream s (ctx);

    // Connect.
    //
    s.expires_after (std::chrono::milliseconds (tr.connect_timeout));
    co_await s.async_connect (addrs, asio::use_awaitable);

    // Prepare Request.
    //
    beast_req br;
    br.method (to_beast_verb (req.method));
    br.target (parts.target);
    br.version (req.version.major * 10 + req.version.minor);

    for (const auto& h : req.headers)
      br.set (h.name, h.value);

    if (req.body)
    {
      br.body () = *req.body;
      br.prepare_payload ();
    }

    // Send.
    //
    s.expires_after (std::chrono::milliseconds (tr.request_timeout));
    co_await http::async_write (s, br, asio::use_awaitable);

    // Receive.
    //
    beast::flat_buffer b;
    beast_res bres;
    co_await http::async_read (s, b, bres, asio::use_awaitable);

    // Shutdown.
    //
    beast::error_code ec;
    ec = s.socket ().shutdown (tcp::socket::shutdown_both, ec);

    // Convert.
    //
    response_type r;
    r.status = from_beast_status (bres.result ());
    r.version = http_version (bres.version () / 10,
                              bres.version () % 10);
    r.reason = string_type (bres.reason ());

    for (const auto& h : bres)
      r.headers.add (string_type (h.name_string ()),
                     string_type (h.value ()));

    if (!bres.body ().empty ())
      r.body = bres.body ();

    co_return r;
  }

  // Download entry point.
  //
  template <typename T>
  asio::awaitable<std::uint64_t>
  basic_http_client<T>::
  download (const string_type& url,
            const string_type& file,
            progress_callback progress,
            std::optional<std::uint64_t> resume)
  {
    co_return co_await download_impl (url, file, progress, resume, 0);
  }

  // Internal download implementation.
  //
  // Unlike the request_* functions which buffer the whole response in memory,
  // here we need to stream the data to a file. We also have to handle
  // potential resuming of interrupted downloads using the Range header.
  //
  template <typename T>
  asio::awaitable<std::uint64_t>
  basic_http_client<T>::
  download_impl (const string_type& url,
                 const string_type& file,
                 progress_callback progress,
                 std::optional<std::uint64_t> resume,
                 std::uint8_t redirect_count)
  {
    using beast_req = http::request<http::empty_body>;

    const auto& tr (session_->traits ());

    if (redirect_count >= tr.max_redirects)
      throw std::runtime_error ("maximum redirects exceeded");

    request_type req (http_method::get, url);

    // If we are resuming, ask the server for the rest of the file.
    //
    if (resume)
    {
      req.set_header (
        string_type ("Range"),
        string_type ("bytes=") +
        std::to_string (*resume) +
        string_type ("-"));
    }

    req.normalize ();

    url_parts parts (parse_url (url));
    bool ssl (parts.scheme == "https");

    // Resolve.
    //
    auto& ctx (session_->io_context ());
    tcp::resolver rslv (ctx);
    auto addrs (co_await rslv.async_resolve (
      parts.host, parts.port, asio::use_awaitable));

    // Open the output file.
    //
    // If we are resuming, we append. Otherwise, we truncate so that we don't
    // leave garbage at the end if the file already existed.
    //
    std::ios_base::openmode mode (std::ios::binary | std::ios::out);
    if (resume)
      mode |= std::ios::app;
    else
      mode |= std::ios::trunc;

    std::ofstream ofs (file, mode);
    if (!ofs)
      throw std::runtime_error ("failed to open file for writing");

    std::uint64_t transferred (resume ? *resume : 0);
    std::uint64_t total (0);

    // Implementation Note:
    //
    // Ideally, we would template the stream logic below to avoid duplicating
    // the SSL vs TCP code. However, Beast streams and SSL streams have
    // slightly different APIs and lifetime requirements that make a common
    // template clumsy to write and maintain.
    //
    // So for now, we accept the duplication for the sake of clarity. If this
    // logic grows any more complex, we should revisit this.
    //
    if (ssl)
    {
      using stream_type = beast::ssl_stream<beast::tcp_stream>;
      stream_type s (ctx, session_->ssl_context ());

      if (!SSL_set_tlsext_host_name (s.native_handle (), parts.host.c_str ()))
      {
        int v (static_cast<int> (::ERR_get_error ()));
        beast::error_code ec (v, asio::error::get_ssl_category ());
        throw beast::system_error (ec, "Failed to set SNI hostname");
      }

      auto& layer (beast::get_lowest_layer (s));
      layer.expires_after (std::chrono::milliseconds (tr.connect_timeout));

      co_await layer.async_connect (addrs, asio::use_awaitable);

      co_await s.async_handshake (
        ssl::stream_base::client, asio::use_awaitable);

      // Send Request.
      //
      beast_req br;
      br.method (http::verb::get);
      br.target (parts.target);
      br.version (11);
      br.set (http::field::host, parts.host);

      for (const auto& h : req.headers)
        br.set (h.name, h.value);

      layer.expires_after (std::chrono::milliseconds (tr.request_timeout));

      co_await http::async_write (s, br, asio::use_awaitable);

      // Use a buffer_body parser to read the response body in small chunks into
      // our local buffer and write them to the file immediately.
      //
      beast::flat_buffer b;
      http::response_parser<http::buffer_body> p;
      p.body_limit (std::numeric_limits<std::uint64_t>::max ());

      co_await http::async_read_header (s, b, p, asio::use_awaitable);

      // Inspect for redirects before reading the body.
      //
      auto status (p.get ().result_int ());

      if (tr.follow_redirects && status >= 300 && status < 400)
      {
        auto loc (p.get ()[http::field::location]);
        if (!loc.empty ())
        {
          ofs.close ();

          beast::error_code ec;
          ec = s.shutdown (ec);

          co_return co_await download_impl (string_type (loc),
                                            file,
                                            progress,
                                            resume,
                                            redirect_count + 1);
        }
      }

      if (status != 200 && status != 206)
        throw std::runtime_error ("download failed with status: " +
                                  std::to_string (status));

      if (p.content_length ())
        total = *p.content_length () + transferred;

      char dbuf[8192];
      p.get ().body ().data = dbuf;
      p.get ().body ().size = sizeof (dbuf);

      while (!p.is_done ())
      {
        std::size_t n (
          co_await http::async_read_some (s, b, p, asio::use_awaitable));

        if (n > 0)
        {
          ofs.write (dbuf, n);
          transferred += n;

          if (progress)
            progress (transferred, total);

          // Reset the buffer for the next chunk.
          //
          p.get ().body ().data = dbuf;
          p.get ().body ().size = sizeof (dbuf);
        }
      }

      ofs.flush ();
    }
    else
    {
      // TCP/Plaintext Path.
      //
      // This duplicates the logic above but operates on a plain tcp_stream.
      //
      beast::tcp_stream s (ctx);

      s.expires_after (std::chrono::milliseconds (tr.connect_timeout));

      co_await s.async_connect (addrs, asio::use_awaitable);

      beast_req br;
      br.method (http::verb::get);
      br.target (parts.target);
      br.version (11);
      br.set (http::field::host, parts.host);

      for (const auto& h : req.headers)
        br.set (h.name, h.value);

      s.expires_after (std::chrono::milliseconds (tr.request_timeout));

      co_await http::async_write (s, br, asio::use_awaitable);

      beast::flat_buffer b;
      http::response_parser<http::buffer_body> p;
      p.body_limit (std::numeric_limits<std::uint64_t>::max ());

      co_await http::async_read_header (s, b, p, asio::use_awaitable);

      auto status (p.get ().result_int ());

      // Handle redirects.
      //
      if (tr.follow_redirects && status >= 300 && status < 400)
      {
        auto loc (p.get () [http::field::location]);
        if (!loc.empty ())
        {
          beast::error_code ec;
          ec = s.socket ().shutdown (tcp::socket::shutdown_both, ec);

          ofs.close ();

          co_return co_await download_impl (string_type (loc),
                                            file,
                                            progress,
                                            resume,
                                            redirect_count + 1);
        }
      }

      if (status != 200 && status != 206)
        throw std::runtime_error ("download failed with status: " +
                                  std::to_string (status));

      if (p.content_length ())
        total = *p.content_length () + transferred;

      char dbuf[8192];
      p.get ().body ().data = dbuf;
      p.get ().body ().size = sizeof (dbuf);

      while (!p.is_done ())
      {
        std::size_t n (
          co_await http::async_read_some (s, b, p, asio::use_awaitable));

        if (n > 0)
        {
          ofs.write (dbuf, n);
          transferred += n;

          if (progress)
            progress (transferred, total);

          p.get ().body ().data = dbuf;
          p.get ().body ().size = sizeof (dbuf);
        }
      }

      ofs.flush ();

      beast::error_code ec;
      ec = s.socket ().shutdown (tcp::socket::shutdown_both, ec);
    }

    ofs.close ();

    if (!ofs)
      throw std::runtime_error ("failed to write file");

    co_return transferred;
  }
}
