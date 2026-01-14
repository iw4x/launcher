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

namespace launcher
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

        // Note that we must explicitly update the Host header for the new
        // location. Failed that, the copied headers retain the old Host (e.g.,
        // github.com), causing 500 errors when hitting the CDN
        // (release-assets...).
        //
        url_parts new_parts (parse_url (*loc));
        next_req.set_header ("Host", new_parts.host);

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
    // We attempt to shut down the SSL layer, but note that we explicitly ignore
    // the error code here. The reality is that many servers simply close the
    // TCP connection after sending the response without performing a proper TLS
    // shutdown sequence. Treating that as a failure would cause us to throw
    // errors on perfectly valid requests.
    //
    // ... Turn out that even attempting an SSL shutdown in this case can
    // block until timeout, so disable it altogether.
    //
    // beast::error_code ec;
    // co_await s.async_shutdown (
    //   asio::redirect_error (asio::use_awaitable, ec));

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
            std::optional<std::uint64_t> resume,
            std::uint64_t rate_limit_bytes_per_second)
  {
    co_return co_await download_impl (url, file, progress, resume, rate_limit_bytes_per_second, 0);
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
                 std::uint64_t rate_limit_bytes_per_second,
                 std::uint8_t redirect_count)
  {
    using namespace std::chrono;
    using parser_type = http::response_parser<http::buffer_body>;

    const auto& tr (session_->traits ());

    if (redirect_count >= tr.max_redirects)
      throw std::runtime_error ("maximum redirects exceeded");

    // Prepare the request.
    //
    // If we are resuming a download, we need to instruct the server to skip
    // the bytes we already have. Note that the Range header is inclusive,
    // so we request from the current size onwards.
    //
    request_type req (http_method::get, url);

    if (resume)
      req.set_header (string_type ("Range"),
                      string_type ("bytes=") +
                      std::to_string (*resume) +
                      string_type ("-"));

    req.normalize ();

    url_parts parts (parse_url (url));
    bool ssl (parts.scheme == "https");

    // Resolve.
    //
    auto& ctx (session_->io_context ());
    tcp::resolver rslv (ctx);
    auto addrs (co_await rslv.async_resolve (parts.host,
                                             parts.port,
                                             asio::use_awaitable));

    // Open the output file.
    //
    // If we are resuming, we append. Otherwise, we truncate so that we don't
    // leave garbage at the end if the file already existed.
    //
    std::ios_base::openmode mode (std::ios::binary | std::ios::out);
    mode |= (resume ? std::ios::app : std::ios::trunc);

    std::ofstream ofs (file, mode);
    if (!ofs)
      throw std::runtime_error ("failed to open file for writing");

    std::uint64_t off (resume ? *resume : 0);
    std::uint64_t tot (0);

    // Common transfer logic for both SSL and TCP streams.
    //
    auto transfer = [&] (auto& s) -> asio::awaitable<std::uint64_t>
    {
      // Note that need to access the lowest layer (the TCP socket) to set
      // timeouts, regardless of whether there is an SSL layer on top.
      //
      auto& layer (beast::get_lowest_layer (s));

      // Send Request.
      //
      http::request<http::empty_body> br;
      br.method (http::verb::get);
      br.target (parts.target);
      br.version (11);
      br.set (http::field::host, parts.host);

      for (const auto& h : req.headers)
        br.set (h.name, h.value);

      layer.expires_after (milliseconds (tr.request_timeout));
      co_await http::async_write (s, br, asio::use_awaitable);

      // Read response body in chunks.
      //
      beast::flat_buffer b;
      parser_type p;
      p.body_limit (std::numeric_limits<std::uint64_t>::max ());

      co_await http::async_read_header (s, b, p, asio::use_awaitable);

      // Handle Redirects.
      //
      // That is, if the server asks us to go elsewhere, we must close the
      // current file handle (to flush buffers) and recursively call ourselves
      // with the new URL.
      //
      auto status (p.get ().result_int ());

      if (tr.follow_redirects && status >= 300 && status < 400)
      {
        auto loc (p.get ()[http::field::location]);
        if (!loc.empty ())
        {
          ofs.close ();

          // Note that we don't need to explicitly shutdown the socket here
          // because we are about to detach from this stack frame. The
          // destructor of the stream `s` will handle the cleanup.
          //
          co_return co_await download_impl (string_type (loc),
                                            file,
                                            progress,
                                            resume,
                                            rate_limit_bytes_per_second,
                                            redirect_count + 1);
        }
      }

      // Check for errors. Note that 206 Partial Content is valid if we
      // requested a Range.
      //
      if (status != 200 && status != 206)
        throw std::runtime_error ("download failed with status: " +
                                  std::to_string (status));

      if (p.content_length ())
        tot = *p.content_length () + off;

      char dbuf[8192];
      p.get ().body ().data = dbuf;
      p.get ().body ().size = sizeof (dbuf);

      auto start (steady_clock::now ());
      std::uint64_t trans (0); // bytes transferred in current window.
      auto lim (rate_limit_bytes_per_second);

      while (!p.is_done ())
      {
        // Reset timeout to keep connection alive while data flows.
        //
        // This fixes a bug where large downloads could still hit the request
        // timeout for some users, causing the transfer to be aborted
        // mid-download.
        //
        layer.expires_after (milliseconds (tr.request_timeout));

        std::size_t n (
          co_await http::async_read_some (s, b, p, asio::use_awaitable));

        if (n > 0)
        {
          ofs.write (dbuf, n);
          off += n;
          trans += n;

          if (progress)
            progress (off, tot);

          // Throttle the transfer since we are saturating the VPS uplink.
          //
          // Note that this is not an arbitrary limit but a hard cap on the
          // host side. And since upgrading the plan is too expensive for a
          // non-commercial project, we have to make do with what we have.
          //
          if (lim > 0)
          {
            auto now (steady_clock::now ());
            auto el (duration_cast<milliseconds> (now - start).count ());
            auto exp ((trans * 1000) / lim);

            if (el < exp)
            {
              // We are running ahead of schedule so throttle.
              //
              asio::steady_timer t (session_->io_context (),
                                    milliseconds (exp - el));
              co_await t.async_wait (asio::use_awaitable);
            }

            // Reset the accounting window every second to prevent drift.
            //
            if (el >= 1000)
              start = now, trans = 0;
          }

          p.get ().body ().data = dbuf;
          p.get ().body ().size = sizeof (dbuf);
        }
      }

      ofs.flush ();
      co_return off;
    };

    // Connection setup.
    //
    // Depending on the scheme, we either instantiate a plain TCP stream or
    // an SSL stream. Note that for SSL, we must perform the handshake
    // *before* we can start talking HTTP.
    //
    if (ssl)
    {
      using stream = beast::ssl_stream<beast::tcp_stream>;
      stream s (ctx, session_->ssl_context ());

      // We must set the SNI hostname, otherwise many modern servers (like
      // Cloudflare) will reject the handshake.
      //
      if (!SSL_set_tlsext_host_name (s.native_handle (), parts.host.c_str ()))
      {
        beast::error_code ec (static_cast<int> (::ERR_get_error ()),
                              asio::error::get_ssl_category ());
        throw beast::system_error (ec, "Failed to set SNI hostname");
      }

      auto& layer (beast::get_lowest_layer (s));
      layer.expires_after (milliseconds (tr.connect_timeout));

      co_await layer.async_connect (addrs, asio::use_awaitable);
      co_await s.async_handshake (ssl::stream_base::client,
                                  asio::use_awaitable);

      auto r (co_await transfer (s));

      // Close the SSL stream. We don't wait for async_shutdown to complete
      // because many servers don't send close_notify properly, instead, we just
      // close the underlying socket.
      //
      beast::error_code ec;
      beast::get_lowest_layer (s).socket ().shutdown (tcp::socket::shutdown_both, ec);
      co_return r;
    }
    else
    {
      beast::tcp_stream s (ctx);
      s.expires_after (milliseconds (tr.connect_timeout));
      co_await s.async_connect (addrs, asio::use_awaitable);

      auto r (co_await transfer (s));

      beast::error_code ec;
      s.socket ().shutdown (tcp::socket::shutdown_both, ec);
      co_return r;
    }
  }
}
