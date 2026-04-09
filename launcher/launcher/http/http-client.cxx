#include <launcher/http/http-client.hxx>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <vector>

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
  namespace fs = std::filesystem;
  namespace http_beast = beast::http;
  using tcp = asio::ip::tcp;

  // SOCKS5 protocol constants (RFC 1928 / RFC 1929).
  //
  constexpr uint8_t socks5_version       = 0x05;
  constexpr uint8_t socks5_auth_none     = 0x00;
  constexpr uint8_t socks5_auth_userpass = 0x02;
  constexpr uint8_t socks5_auth_failed   = 0xFF;
  constexpr uint8_t socks5_cmd_connect   = 0x01;
  constexpr uint8_t socks5_atyp_domain   = 0x03;
  constexpr uint8_t socks5_userpass_ver  = 0x01;
  constexpr uint8_t socks5_reply_ok      = 0x00;

  // Size of the buffer used when streaming download bodies.
  //
  constexpr size_t download_buffer_size  = 8192;

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

    // Return true if the proxy URL uses a SOCKS scheme (socks5, socks5h,
    // socks4, socks4a).
    //
    bool
    is_socks_proxy (const url_parts& p)
    {
      return p.scheme == "socks5"  || p.scheme == "socks5h" ||
             p.scheme == "socks4a" || p.scheme == "socks4";
    }

    // Parse optional userinfo (user:pass) from the proxy URL.
    //
    // The standard form is socks5://user:pass@host:port.
    //
    struct proxy_userinfo
    {
      string user;
      string pass;
    };

    proxy_userinfo
    parse_proxy_userinfo (const string& proxy_url)
    {
      proxy_userinfo r;

      size_t p (0);
      size_t si (proxy_url.find ("://", p));
      if (si != string::npos) p = si + 3;

      size_t at (proxy_url.find ('@', p));
      if (at == string::npos) return r;

      // Make sure the '@' comes before the host portion (before any /).
      //
      size_t sl (proxy_url.find ('/', p));
      if (sl != string::npos && at > sl) return r;

      string info (proxy_url.substr (p, at - p));
      size_t c (info.find (':'));

      if (c != string::npos)
      {
        r.user = info.substr (0, c);
        r.pass = info.substr (c + 1);
      }
      else
        r.user = info;

      return r;
    }

    // Parse a proxy URL into url_parts, stripping any userinfo component
    // so that host/port are extracted correctly.
    //
    url_parts
    parse_proxy_url (const string& proxy_url)
    {
      // Remove userinfo if present
      //
      // scheme://user:pass@host:port -> scheme://host:port
      //
      string u (proxy_url);

      size_t p (0);
      size_t si (u.find ("://", p));
      if (si != string::npos) p = si + 3;

      size_t at (u.find ('@', p));
      if (at != string::npos)
      {
        size_t sl (u.find ('/', p));
        if (sl == string::npos || at < sl)
          u.erase (p, at - p + 1);
      }

      return parse_url (u);
    }

    // TCP connection through an HTTP proxy using the CONNECT method. On success
    // the returned tcp_stream is connected to the proxy and ready for
    // tunnelling (e.g. TLS handshake to the origin).
    //
    asio::awaitable<void>
    proxy_connect (beast::tcp_stream& stream,
                   asio::io_context& ioc,
                   const url_parts& proxy,
                   const string& dest_host,
                   const string& dest_port,
                   chrono::milliseconds connect_timeout,
                   chrono::milliseconds request_timeout)
    {
      tcp::resolver rv (ioc);
      auto eps (co_await rv.async_resolve (
        proxy.host, proxy.port, asio::use_awaitable));

      stream.expires_after (connect_timeout);
      co_await stream.async_connect (eps, asio::use_awaitable);

      // Ask the proxy to open a tunnel to the destination.
      //
      string authority (dest_host + ":" + dest_port);

      http_beast::request<http_beast::empty_body> creq (
        http_beast::verb::connect, authority, 11);
      creq.set (http_beast::field::host, authority);

      stream.expires_after (request_timeout);
      co_await http_beast::async_write (stream, creq, asio::use_awaitable);

      beast::flat_buffer buf;
      http_beast::response<http_beast::empty_body> cres;
      co_await http_beast::async_read (stream, buf, cres, asio::use_awaitable);

      if (cres.result () != http_beast::status::ok)
        throw runtime_error (
          "proxy CONNECT failed: " + std::to_string (cres.result_int ()));
    }

    // TCP connection through a SOCKS5 proxy (RFC 1928).
    //
    // Supports both no-authentication and username/password authentication (RFC
    // 1929). On success the tcp_stream is connected through the proxy to
    // dest_host:dest_port and ready for use (e.g. TLS handshake).
    //
    asio::awaitable<void>
    socks5_connect (beast::tcp_stream& stream,
                    asio::io_context& ioc,
                    const url_parts& proxy,
                    const string& proxy_url,
                    const string& dest_host,
                    const string& dest_port,
                    chrono::milliseconds connect_timeout,
                    chrono::milliseconds request_timeout)
    {
      // Connect to the SOCKS5 proxy server.
      //
      tcp::resolver rv (ioc);
      auto eps (co_await rv.async_resolve (
        proxy.host, proxy.port, asio::use_awaitable));

      stream.expires_after (connect_timeout);
      co_await stream.async_connect (eps, asio::use_awaitable);

      stream.expires_after (request_timeout);

      proxy_userinfo ui (parse_proxy_userinfo (proxy_url));
      bool has_auth (!ui.user.empty ());

      // Client greeting (RFC 1928 §4)
      //
      // | VER | NMETHODS | METHODS  |
      // | 1   | 1        | 1 to 255 |
      //
      uint8_t greeting[4];
      greeting[0] = socks5_version;

      if (has_auth)
      {
        greeting[1] = 0x02;                        // 2 methods
        greeting[2] = socks5_auth_none;
        greeting[3] = socks5_auth_userpass;
      }
      else
      {
        greeting[1] = 0x01;                        // 1 method
        greeting[2] = socks5_auth_none;
      }

      size_t glen (has_auth ? 4 : 3);
      co_await asio::async_write (
        stream, asio::buffer (greeting, glen), asio::use_awaitable);

      // Server method selection
      //
      // | VER | METHOD |
      // | 1   | 1      |
      //
      uint8_t choice[2];
      co_await asio::async_read (
        stream, asio::buffer (choice, 2), asio::use_awaitable);

      if (choice[0] != socks5_version)
        throw runtime_error ("SOCKS5 proxy returned invalid version");

      if (choice[1] == socks5_auth_failed)
        throw runtime_error (
          "SOCKS5 proxy: no acceptable authentication method");

      // Username/password sub-negotiation (RFC 1929)
      //
      if (choice[1] == socks5_auth_userpass)
      {
        if (!has_auth)
          throw runtime_error (
            "SOCKS5 proxy requires authentication but no credentials provided");

        // | VER | ULEN | UNAME    | PLEN | PASSWD   |
        // | 1   | 1    | 1 to 255 | 1    | 1 to 255 |
        //
        if (ui.user.size () > 255 || ui.pass.size () > 255)
          throw runtime_error (
            "SOCKS5 proxy credentials too long (max 255 bytes each)");

        vector<uint8_t> auth;
        auth.reserve (3 + ui.user.size () + ui.pass.size ());
        auth.push_back (socks5_userpass_ver);
        auth.push_back (static_cast<uint8_t> (ui.user.size ()));
        auth.insert (auth.end (), ui.user.begin (), ui.user.end ());
        auth.push_back (static_cast<uint8_t> (ui.pass.size ()));
        auth.insert (auth.end (), ui.pass.begin (), ui.pass.end ());

        co_await asio::async_write (
          stream, asio::buffer (auth), asio::use_awaitable);

        // | VER | STATUS |
        // | 1   | 1      |
        //
        uint8_t aresp[2];
        co_await asio::async_read (
          stream, asio::buffer (aresp, 2), asio::use_awaitable);

        if (aresp[1] != socks5_reply_ok)
          throw runtime_error ("SOCKS5 proxy authentication failed");
      }
      else if (choice[1] != socks5_auth_none)
      {
        throw runtime_error (
          "SOCKS5 proxy selected unsupported auth method: " +
          std::to_string (static_cast<int> (choice[1])));
      }

      // CONNECT request (RFC 1928 §4)
      //
      // | VER | CMD | RSV   | ATYP | DST.ADDR | DST.PORT |
      // | 1   | 1   | X'00' | 1    | Variable | 2        |
      //
      if (dest_host.size () > 255)
        throw runtime_error ("SOCKS5: destination hostname too long");

      uint16_t port (static_cast<uint16_t> (stoi (dest_port)));
      uint8_t port_hi (static_cast<uint8_t> (port >> 8));
      uint8_t port_lo (static_cast<uint8_t> (port & 0xFF));

      vector<uint8_t> creq;
      creq.reserve (7 + dest_host.size ());
      creq.push_back (socks5_version);
      creq.push_back (socks5_cmd_connect);
      creq.push_back (0x00);                                   // RSV
      creq.push_back (socks5_atyp_domain);
      creq.push_back (static_cast<uint8_t> (dest_host.size ()));
      creq.insert (creq.end (), dest_host.begin (), dest_host.end ());
      creq.push_back (port_hi);
      creq.push_back (port_lo);

      co_await asio::async_write (
        stream, asio::buffer (creq), asio::use_awaitable);

      // CONNECT reply
      //
      // | VER | REP | RSV   | ATYP | BND.ADDR | BND.PORT |
      // | 1   | 1   | X'00' | 1    | Variable | 2        |
      //
      // Read the fixed 4-byte header first, then consume the variable-length
      // bound address + 2-byte port.
      //
      uint8_t hdr[4];
      co_await asio::async_read (
        stream, asio::buffer (hdr, 4), asio::use_awaitable);

      if (hdr[0] != socks5_version)
        throw runtime_error ("SOCKS5 proxy returned invalid version in reply");

      if (hdr[1] != socks5_reply_ok)
      {
        static const char* const errs[] = {
          "succeeded",
          "general SOCKS server failure",
          "connection not allowed by ruleset",
          "network unreachable",
          "host unreachable",
          "connection refused",
          "TTL expired",
          "command not supported",
          "address type not supported"
        };

        const char* msg (hdr[1] < 9 ? errs[hdr[1]] : "unknown error");
        throw runtime_error (
          string ("SOCKS5 proxy connect failed: ") + msg);
      }

      // Drain the bound address so the stream is clean for the caller.
      //
      size_t drain (0);
      switch (hdr[3])
      {
        case 0x01: drain = 4 + 2; break;   // IPv4 + port
        case 0x04: drain = 16 + 2; break;  // IPv6 + port
        case 0x03: // Domain: 1-byte length + name + port
        {
          uint8_t len;
          co_await asio::async_read (
            stream, asio::buffer (&len, 1), asio::use_awaitable);
          drain = len + 2;
          break;
        }
        default:
          throw runtime_error ("SOCKS5 proxy returned unknown address type");
      }

      vector<uint8_t> sink (drain);
      co_await asio::async_read (
        stream, asio::buffer (sink), asio::use_awaitable);
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
    : session_ (c, http_client_traits ())
  {
  }

  http_client::
  http_client (asio::io_context& c, const http_client_traits& t)
    : session_ (c, t)
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
    const auto& t (session_.traits ());

    // Bail out if we're caught in a redirect loop.
    //
    if (rc >= t.max_redirects)
      throw runtime_error ("maximum redirects exceeded");

    url_parts p (parse_url (rq.url));
    bool s (p.scheme == "https");

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

    auto& c (session_.io_context ());
    auto& x (session_.ssl_context ());
    const auto& t (session_.traits ());

    url_parts p (parse_url (rq.url));

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

    if (!t.proxy_url.empty ())
    {
      // Tunnel through the proxy, then TLS-handshake.
      //
      url_parts pp (parse_proxy_url (t.proxy_url));

      if (is_socks_proxy (pp))
        co_await socks5_connect (l, c, pp, t.proxy_url,
                                 p.host, p.port,
                                 t.connect_timeout, t.request_timeout);
      else
        co_await proxy_connect (l, c, pp, p.host, p.port,
                                t.connect_timeout, t.request_timeout);
    }
    else
    {
      tcp::resolver rv (c);
      auto as (co_await rv.async_resolve (
        p.host, p.port, asio::use_awaitable));

      l.expires_after (t.connect_timeout);
      co_await l.async_connect (as, asio::use_awaitable);
    }

    // Establish the connection and perform the handshake.
    //
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

    l.expires_after (t.request_timeout);

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

    auto& c (session_.io_context ());
    const auto& t (session_.traits ());

    url_parts p (parse_url (rq.url));

    beast::tcp_stream s (c);

    // SOCKS proxies are transparent tunnels, so the request target is
    // always the path.  HTTP proxies require an absolute URI.
    //
    bool use_absolute_target (false);

    if (!t.proxy_url.empty ())
    {
      url_parts pp (parse_proxy_url (t.proxy_url));

      if (is_socks_proxy (pp))
      {
        // SOCKS tunnel: after handshake the stream looks like a direct
        // connection to the origin.
        //
        co_await socks5_connect (s, c, pp, t.proxy_url,
                                 p.host, p.port,
                                 t.connect_timeout, t.request_timeout);
      }
      else
      {
        // HTTP proxy: connect to the proxy and use an absolute URI as
        // the request target.
        //
        tcp::resolver rv (c);
        auto as (co_await rv.async_resolve (
          pp.host, pp.port, asio::use_awaitable));

        s.expires_after (t.connect_timeout);
        co_await s.async_connect (as, asio::use_awaitable);

        use_absolute_target = true;
      }
    }
    else
    {
      tcp::resolver rv (c);
      auto as (co_await rv.async_resolve (
        p.host, p.port, asio::use_awaitable));

      s.expires_after (t.connect_timeout);
      co_await s.async_connect (as, asio::use_awaitable);
    }

    beast_req br;
    br.method (to_beast_verb (rq.method));
    br.target (use_absolute_target ? rq.url : p.target);
    br.version (rq.version.major * 10 + rq.version.minor);

    for (const auto& h : rq.headers)
      br.set (h.name, h.value);

    if (rq.body)
    {
      br.body () = *rq.body;
      br.prepare_payload ();
    }

    s.expires_after (t.request_timeout);

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
            const fs::path& f,
            progress_callback cb,
            optional<uint64_t> rs,
            uint64_t rl)
  {
    co_return co_await download_impl (u, f, cb, rs, rl, 0);
  }

  asio::awaitable<uint64_t> http_client::
  download_impl (const string& u,
                 const fs::path& f,
                 progress_callback cb,
                 optional<uint64_t> rs,
                 uint64_t rl,
                 uint8_t rc)
  {
    using namespace chrono;
    using parser = http_beast::response_parser<http_beast::buffer_body>;

    const auto& t (session_.traits ());

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

    auto& c (session_.io_context ());

    ios_base::openmode m (ios::binary | ios::out);
    m |= (rs ? ios::app : ios::trunc);

    ofstream os (f, m);

    if (!os)
      throw runtime_error ("failed to open file for writing");

    uint64_t off (rs ? *rs : 0);
    uint64_t tot (0);

    // Generic transfer lambda to handle both plaintext and TLS streams uniformly.
    //
    // For plaintext HTTP via a proxy the request target must be an absolute
    // URI; for everything else (direct or tunnelled) we use the path.
    //
    auto transfer = [&] (auto& s, bool absolute_target = false) -> asio::awaitable<uint64_t>
    {
      auto& l (beast::get_lowest_layer (s));

      http_beast::request<http_beast::empty_body> br;
      br.method (http_beast::verb::get);
      br.target (absolute_target ? u : p.target);
      br.version (11);
      br.set (http_beast::field::host, p.host);

      for (const auto& h : rq.headers)
        br.set (h.name, h.value);

      l.expires_after (t.request_timeout);
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

      char db[download_buffer_size];
      ps.get ().body ().data = db;
      ps.get ().body ().size = sizeof (db);

      auto start (steady_clock::now ());
      uint64_t tr (0);

      // Stream the body chunks into the file.
      //
      while (!ps.is_done ())
      {
        l.expires_after (t.request_timeout);

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
      stream s (c, session_.ssl_context ());

      if (!SSL_set_tlsext_host_name (s.native_handle (), p.host.c_str ()))
      {
        beast::error_code ec (
          static_cast<int> (::ERR_get_error ()),
          asio::error::get_ssl_category ());

        throw beast::system_error (ec, "failed to set SNI hostname");
      }

      auto& l (beast::get_lowest_layer (s));

      if (!t.proxy_url.empty ())
      {
        url_parts pp (parse_proxy_url (t.proxy_url));

        if (is_socks_proxy (pp))
          co_await socks5_connect (l, c, pp, t.proxy_url,
                                   p.host, p.port,
                                   t.connect_timeout, t.request_timeout);
        else
          co_await proxy_connect (l, c, pp, p.host, p.port,
                                  t.connect_timeout, t.request_timeout);
      }
      else
      {
        tcp::resolver rv (c);
        auto as (co_await rv.async_resolve (
          p.host, p.port, asio::use_awaitable));

        l.expires_after (t.connect_timeout);
        co_await l.async_connect (as, asio::use_awaitable);
      }

      co_await s.async_handshake (ssl::stream_base::client, asio::use_awaitable);

      auto r (co_await transfer (s, false));

      beast::error_code ec;
      beast::get_lowest_layer (s).socket ().shutdown (tcp::socket::shutdown_both, ec);

      co_return r;
    }
    else
    {
      beast::tcp_stream s (c);

      bool use_absolute_target (false);

      if (!t.proxy_url.empty ())
      {
        url_parts pp (parse_proxy_url (t.proxy_url));

        if (is_socks_proxy (pp))
        {
          co_await socks5_connect (s, c, pp, t.proxy_url,
                                   p.host, p.port,
                                   t.connect_timeout, t.request_timeout);
        }
        else
        {
          tcp::resolver rv (c);
          auto as (co_await rv.async_resolve (
            pp.host, pp.port, asio::use_awaitable));

          s.expires_after (t.connect_timeout);
          co_await s.async_connect (as, asio::use_awaitable);

          use_absolute_target = true;
        }
      }
      else
      {
        tcp::resolver rv (c);
        auto as (co_await rv.async_resolve (
          p.host, p.port, asio::use_awaitable));

        s.expires_after (t.connect_timeout);
        co_await s.async_connect (as, asio::use_awaitable);
      }

      auto r (co_await transfer (s, use_absolute_target));

      beast::error_code ec;
      s.socket ().shutdown (tcp::socket::shutdown_both, ec);

      co_return r;
    }
  }
}
