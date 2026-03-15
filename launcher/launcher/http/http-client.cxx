#include <launcher/http/http-client.hxx>

#include <fstream>
#include <stdexcept>
#include <iostream>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include <openssl/ssl.h>
#include <openssl/err.h>

using namespace std;

namespace launcher
{
  namespace http_beast = beast::http;
  using tcp = asio::ip::tcp;

  namespace
  {
    struct url_parts
    {
      string scheme;
      string host;
      string port;
      string target;
    };

    // Parse the target URL into discrete components.
    //
    url_parts
    parse_url (const string& u)
    {
      url_parts r;
      size_t p (0);
      size_t i (u.find ("://", p));

      // Figure out the scheme.
      //
      if (i != string::npos)
      {
        r.scheme = u.substr (0, i);
        p = i + 3;
      }
      else
        r.scheme = "http";

      size_t e (u.find ('/', p));

      if (e == string::npos)
        e = u.size ();

      string a (u.substr (p, e - p));
      size_t c (a.find (':'));

      // Extract the host and port.
      //
      if (c != string::npos)
      {
        r.host = a.substr (0, c);
        r.port = a.substr (c + 1);
      }
      else
      {
        r.host = a;
        r.port = (r.scheme == "https") ? "443" : "80";
      }

      // Keep the target path intact.
      //
      if (e < u.size ())
        r.target = u.substr (e);
      else
        r.target = "/";

      return r;
    }

    // Map our method to the Beast verb.
    //
    http_beast::verb
    to_beast_verb (http_method m)
    {
      switch (m)
      {
        case http_method::get:     return http_beast::verb::get;
        case http_method::head:    return http_beast::verb::head;
        case http_method::post:    return http_beast::verb::post;
        case http_method::put:     return http_beast::verb::put;
        case http_method::delete_: return http_beast::verb::delete_;
        case http_method::connect: return http_beast::verb::connect;
        case http_method::options: return http_beast::verb::options;
        case http_method::trace:   return http_beast::verb::trace;
        case http_method::patch:   return http_beast::verb::patch;
      }
      return http_beast::verb::get;
    }

    http_status
    from_beast_status (http_beast::status s)
    {
      return static_cast<http_status> (static_cast<uint16_t> (s));
    }
  }

  http_session::
  http_session (asio::io_context& c, const http_client_traits& t)
    : ioc_ (c),
      traits_ (t),
      ssl_ctx_ (ssl::context::tlsv12_client)
  {
    configure_ssl ();
  }

  void http_session::
  configure_ssl ()
  {
    // Load certificates if provided, otherwise fallback to defaults.
    //
    if (!traits_.ssl_cert_file.empty ())
      ssl_ctx_.load_verify_file (traits_.ssl_cert_file);
    else
      ssl_ctx_.set_default_verify_paths ();

    // Toggle peer verification based on traits.
    //
    ssl_ctx_.set_verify_mode (
      traits_.verify_ssl ? ssl::verify_peer : ssl::verify_none);

    // Apply standard security workarounds and disable old protocols.
    //
    ssl_ctx_.set_options (ssl::context::default_workarounds |
                          ssl::context::no_sslv2 |
                          ssl::context::no_sslv3 |
                          ssl::context::single_dh_use);

    SSL_CTX_set_tlsext_servername_callback (
      ssl_ctx_.native_handle (), nullptr);
  }

  http_client::
  http_client (asio::io_context& c)
    : session_ (make_unique<http_session> (c, http_client_traits ()))
  {
  }

  http_client::
  http_client (asio::io_context& c, const http_client_traits& t)
    : session_ (make_unique<http_session> (c, t))
  {
  }

  asio::awaitable<http_response> http_client::
  get (const string& u)
  {
    http_request rq (http_method::get, u);
    rq.normalize ();
    co_return co_await request (rq);
  }

  asio::awaitable<http_response> http_client::
  post (const string& u, const string& b, const string& ct)
  {
    http_request rq (http_method::post, u);
    rq.set_content_type (ct);
    rq.set_body (b);
    rq.normalize ();
    co_return co_await request (rq);
  }

  asio::awaitable<http_response> http_client::
  put (const string& u, const string& b, const string& ct)
  {
    http_request rq (http_method::put, u);
    rq.set_content_type (ct);
    rq.set_body (b);
    rq.normalize ();
    co_return co_await request (rq);
  }

  asio::awaitable<http_response> http_client::
  delete_ (const string& u)
  {
    http_request rq (http_method::delete_, u);
    rq.normalize ();
    co_return co_await request (rq);
  }

  asio::awaitable<http_response> http_client::
  head (const string& u)
  {
    http_request rq (http_method::head, u);
    rq.normalize ();
    co_return co_await request (rq);
  }

  asio::awaitable<http_response> http_client::
  request (const http_request& rq)
  {
    co_return co_await request_impl (rq, 0);
  }

  asio::awaitable<http_response> http_client::
  request_impl (http_request rq, uint8_t rc)
  {
    const auto& t (session_->traits ());

    // Bail out if we're caught in a redirect loop.
    //
    if (rc >= t.max_redirects)
      throw runtime_error ("maximum redirects exceeded");

    url_parts p (parse_url (rq.url));
    bool s (p.scheme == "https");

    // Route to the appropriate protocol handler.
    //
    http_response rs (s ? co_await request_ssl (rq) : co_await request_tcp (rq));

    if (t.follow_redirects && rs.is_redirection ())
    {
      auto l (rs.location ());

      if (l)
      {
        http_request n (rq.method, *l, rq.version);
        n.headers = rq.headers;

        url_parts np (parse_url (*l));
        n.set_header ("Host", np.host);

        // Downgrade POST/PUT to GET on 303 See Other.
        //
        if (rs.status == http_status::see_other)
        {
          if (rq.method == http_method::post || rq.method == http_method::put)
          {
            n.method = http_method::get;
            n.body = nullopt;
          }
        }

        n.normalize ();
        co_return co_await request_impl (move (n), rc + 1);
      }
    }

    co_return rs;
  }

  asio::awaitable<http_response> http_client::
  request_ssl (const http_request& rq)
  {
    using stream = beast::ssl_stream<beast::tcp_stream>;
    using beast_req = http_beast::request<http_beast::string_body>;
    using beast_res = http_beast::response<http_beast::string_body>;

    auto& c (session_->io_context ());
    auto& x (session_->ssl_context ());
    const auto& t (session_->traits ());

    url_parts p (parse_url (rq.url));

    tcp::resolver rv (c);
    auto as (co_await rv.async_resolve (p.host, p.port, asio::use_awaitable));

    stream s (c, x);

    // Configure SNI for TLS.
    //
    if (!SSL_set_tlsext_host_name (s.native_handle (), p.host.c_str ()))
    {
      int e (static_cast<int> (::ERR_get_error ()));
      beast::error_code ec (e, asio::error::get_ssl_category ());
      throw beast::system_error (ec, "failed to set SNI hostname");
    }

    auto& l (beast::get_lowest_layer (s));
    l.expires_after (chrono::milliseconds (t.connect_timeout));

    // Establish the connection and perform the handshake.
    //
    co_await l.async_connect (as, asio::use_awaitable);
    co_await s.async_handshake (ssl::stream_base::client, asio::use_awaitable);

    beast_req br;
    br.method (to_beast_verb (rq.method));
    br.target (rq.target ());
    br.version (rq.version.major * 10 + rq.version.minor);

    for (const auto& h : rq.headers)
      br.set (h.name, h.value);

    // Assign body if it exists.
    //
    if (rq.body)
    {
      br.body () = *rq.body;
      br.prepare_payload ();
    }

    l.expires_after (chrono::milliseconds (t.request_timeout));

    // Dispatch the request.
    //
    co_await http_beast::async_write (s, br, asio::use_awaitable);

    beast::flat_buffer b;
    beast_res bres;

    // Read the response back.
    //
    co_await http_beast::async_read (s, b, bres, asio::use_awaitable);

    http_response rs;
    rs.status  = from_beast_status (bres.result ());
    rs.version = http_version (bres.version () / 10, bres.version () % 10);
    rs.reason  = string (bres.reason ());

    for (const auto& h : bres)
      rs.headers.add (string (h.name_string ()), string (h.value ()));

    if (!bres.body ().empty ())
      rs.body = bres.body ();

    co_return rs;
  }

  asio::awaitable<http_response> http_client::
  request_tcp (const http_request& rq)
  {
    using beast_req = http_beast::request<http_beast::string_body>;
    using beast_res = http_beast::response<http_beast::string_body>;

    auto& c (session_->io_context ());
    const auto& t (session_->traits ());

    url_parts p (parse_url (rq.url));

    tcp::resolver rv (c);
    auto as (co_await rv.async_resolve (p.host, p.port, asio::use_awaitable));

    beast::tcp_stream s (c);

    s.expires_after (chrono::milliseconds (t.connect_timeout));
    co_await s.async_connect (as, asio::use_awaitable);

    beast_req br;
    br.method (to_beast_verb (rq.method));
    br.target (p.target);
    br.version (rq.version.major * 10 + rq.version.minor);

    for (const auto& h : rq.headers)
      br.set (h.name, h.value);

    if (rq.body)
    {
      br.body () = *rq.body;
      br.prepare_payload ();
    }

    s.expires_after (chrono::milliseconds (t.request_timeout));

    // Send over the wire.
    //
    co_await http_beast::async_write (s, br, asio::use_awaitable);

    beast::flat_buffer b;
    beast_res bres;

    // Collect the response.
    //
    co_await http_beast::async_read (s, b, bres, asio::use_awaitable);

    beast::error_code ec;
    s.socket ().shutdown (tcp::socket::shutdown_both, ec);

    http_response rs;
    rs.status = from_beast_status (bres.result ());
    rs.version = http_version (bres.version () / 10, bres.version () % 10);
    rs.reason = string (bres.reason ());

    for (const auto& h : bres)
      rs.headers.add (string (h.name_string ()), string (h.value ()));

    if (!bres.body ().empty ())
      rs.body = bres.body ();

    co_return rs;
  }

  asio::awaitable<uint64_t> http_client::
  download (const string& u,
            const string& f,
            progress_callback cb,
            optional<uint64_t> rs,
            uint64_t rl)
  {
    co_return co_await download_impl (u, f, cb, rs, rl, 0);
  }

  asio::awaitable<uint64_t> http_client::
  download_impl (const string& u,
                 const string& f,
                 progress_callback cb,
                 optional<uint64_t> rs,
                 uint64_t rl,
                 uint8_t rc)
  {
    using namespace chrono;
    using parser = http_beast::response_parser<http_beast::buffer_body>;

    const auto& t (session_->traits ());

    // Prevent redirect loops.
    //
    if (rc >= t.max_redirects)
      throw runtime_error ("maximum redirects exceeded");

    http_request rq (http_method::get, u);

    // Setup the byte range if we are resuming.
    //
    if (rs)
      rq.set_header ("Range", "bytes=" + std::to_string (*rs) + "-");

    rq.normalize ();

    url_parts p (parse_url (u));
    bool ssl (p.scheme == "https");

    auto& c (session_->io_context ());
    tcp::resolver rv (c);
    auto as (co_await rv.async_resolve (p.host, p.port, asio::use_awaitable));

    ios_base::openmode m (ios::binary | ios::out);
    m |= (rs ? ios::app : ios::trunc);

    ofstream os (f, m);

    if (!os)
      throw runtime_error ("failed to open file for writing");

    uint64_t off (rs ? *rs : 0);
    uint64_t tot (0);

    // Generic transfer lambda to handle both plaintext and TLS streams uniformly.
    //
    auto transfer = [&] (auto& s) -> asio::awaitable<uint64_t>
    {
      auto& l (beast::get_lowest_layer (s));

      http_beast::request<http_beast::empty_body> br;
      br.method (http_beast::verb::get);
      br.target (p.target);
      br.version (11);
      br.set (http_beast::field::host, p.host);

      for (const auto& h : rq.headers)
        br.set (h.name, h.value);

      l.expires_after (milliseconds (t.request_timeout));
      co_await http_beast::async_write (s, br, asio::use_awaitable);

      beast::flat_buffer b;
      parser ps;
      ps.body_limit (numeric_limits<uint64_t>::max ());

      // Only read the header initially so we can check for redirects
      // and setup our buffer before pulling the body.
      //
      co_await http_beast::async_read_header (s, b, ps, asio::use_awaitable);

      auto st (ps.get ().result_int ());

      if (t.follow_redirects && st >= 300 && st < 400)
      {
        auto loc (ps.get ()[http_beast::field::location]);

        if (!loc.empty ())
        {
          os.close ();
          co_return co_await download_impl (
            string (loc), f, cb, rs, rl, rc + 1);
        }
      }

      if (st != 200 && st != 206)
        throw runtime_error (
          "download failed with status: " + std::to_string (st));

      if (ps.content_length ())
        tot = *ps.content_length () + off;

      char db[8192];
      ps.get ().body ().data = db;
      ps.get ().body ().size = sizeof (db);

      auto start (steady_clock::now ());
      uint64_t tr (0);

      // Stream the body chunks into the file.
      //
      while (!ps.is_done ())
      {
        l.expires_after (milliseconds (t.request_timeout));

        size_t n (
          co_await http_beast::async_read_some (s, b, ps, asio::use_awaitable));

        if (n > 0)
        {
          os.write (db, n);
          off += n;
          tr += n;

          if (cb)
            cb (off, tot);

          // Apply rate limiting if requested.
          //
          if (rl > 0)
          {
            auto now (steady_clock::now ());
            auto el (duration_cast<milliseconds> (now - start).count ());
            auto ex ((tr * 1000) / rl);

            if (el < ex)
            {
              asio::steady_timer tm (c, milliseconds (ex - el));
              co_await tm.async_wait (asio::use_awaitable);
            }

            if (el >= 1000)
            {
              start = now;
              tr = 0;
            }
          }

          ps.get ().body ().data = db;
          ps.get ().body ().size = sizeof (db);
        }
      }

      os.flush ();
      co_return off;
    };

    if (ssl)
    {
      using stream = beast::ssl_stream<beast::tcp_stream>;
      stream s (c, session_->ssl_context ());

      if (!SSL_set_tlsext_host_name (s.native_handle (), p.host.c_str ()))
      {
        beast::error_code ec (
          static_cast<int> (::ERR_get_error ()),
          asio::error::get_ssl_category ());

        throw beast::system_error (ec, "failed to set SNI hostname");
      }

      auto& l (beast::get_lowest_layer (s));
      l.expires_after (milliseconds (t.connect_timeout));

      co_await l.async_connect (as, asio::use_awaitable);
      co_await s.async_handshake (ssl::stream_base::client, asio::use_awaitable);

      auto r (co_await transfer (s));

      beast::error_code ec;
      beast::get_lowest_layer (s).socket ().shutdown (tcp::socket::shutdown_both, ec);

      co_return r;
    }
    else
    {
      beast::tcp_stream s (c);

      s.expires_after (milliseconds (t.connect_timeout));
      co_await s.async_connect (as, asio::use_awaitable);

      auto r (co_await transfer (s));

      beast::error_code ec;
      s.socket ().shutdown (tcp::socket::shutdown_both, ec);

      co_return r;
    }
  }
}
