#include <launcher/steam/steam-cm-client.hxx>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/json.hpp>

#include <algorithm>
#include <cassert>
#include <utility>

#include <launcher/launcher-log.hxx>
#include <launcher/steam/steam-compression.hxx>

using namespace std;
using namespace std::chrono_literals;

namespace launcher
{
  namespace
  {
    namespace json = boost::json;
    namespace websocket = beast::websocket;

    using tcp = asio::ip::tcp;

    constexpr auto info ([] (auto&&... a)
    {
      log::info (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr auto warning ([] (auto&&... a)
    {
      log::warning (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr auto trace_l2 ([] (auto&&... a)
    {
      log::trace_l2 (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr auto trace_l3 ([] (auto&&... a)
    {
      log::trace_l3 (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr uint32_t client_protocol_version = 65581;

    constexpr uint32_t client_package_version = 1771;

    constexpr uint32_t client_os_type_windows_10 = 16;

    constexpr const char* public_realm = "steamglobal";

    constexpr const char* cm_directory_url =
      "https://api.steampowered.com/ISteamDirectory/GetCMListForConnect/v1/";

    constexpr const char* cm_websocket_path = "/cmsocket/";

    enum : uint32_t
    {
      logon_protocol_version       = 1,
      logon_cell_id                = 3,
      logon_client_package_version = 5,
      logon_client_language        = 6,
      logon_client_os_type         = 7,
      logon_should_remember_pass   = 8,
      logon_client_supplied_id     = 22,
      logon_machine_id             = 30,
      logon_supports_rate_limit    = 102,
      logon_access_token           = 108
    };

    enum : uint32_t
    {
      logon_response_eresult                   = 1,
      logon_response_out_of_game_heartbeat      = 2,
      logon_response_client_supplied_steamid    = 4
    };

    enum : uint32_t
    {
      license_list_licenses = 2,
      license_package_id    = 1
    };

    enum : uint32_t
    {
      logged_off_eresult = 1
    };

    enum : uint32_t
    {
      hello_protocol_version = 1
    };

    bool
    parse_endpoint (const string& s, string& host, uint16_t& port)
    {
      size_t c (s.rfind (':'));

      if (c == string::npos)
      {
        host = s;
        port = 443;

        return !host.empty ();
      }

      host = s.substr (0, c);

      string p (s.substr (c + 1));

      if (host.empty () || p.empty ())
        return false;

      unsigned long v (0);

      try
      {
        size_t n (0);
        v = stoul (p, &n);

        if (n != p.size ())
          return false;
      }
      catch (const exception&)
      {
        return false;
      }

      if (v == 0 || v > 65535)
        return false;

      port = static_cast<uint16_t> (v);

      return true;
    }
  }

  string steam_cm_endpoint::
  to_string () const
  {
    return host + ":" + std::to_string (port);
  }

  steam_cm_client::connection::
  connection (asio::io_context& c, ssl::context& sc)
    : ws (c, sc),
      loops_event (c),
      heartbeat (c),
      outbox_event (c)
  {
    outbox_event.expires_at (asio::steady_timer::time_point::max ());
    loops_event.expires_at (asio::steady_timer::time_point::max ());
  }

  steam_cm_client::
  steam_cm_client (asio::io_context& c, steam_cm_traits t)
    : ioc_ (c),
      traits_ (move (t)),
      ssl_ctx_ (ssl::context::tlsv12_client),
      licenses_event_ (c),
      logon_event_ (c)
  {
    ssl_ctx_.set_default_verify_paths ();

    ssl_ctx_.set_verify_mode (traits_.verify_ssl ? ssl::verify_peer
                                                 : ssl::verify_none);

    ssl_ctx_.set_options (ssl::context::default_workarounds |
                          ssl::context::no_sslv2 |
                          ssl::context::no_sslv3 |
                          ssl::context::single_dh_use);

    licenses_event_.expires_at (asio::steady_timer::time_point::max ());
    logon_event_.expires_at (asio::steady_timer::time_point::max ());
  }

  steam_cm_client::
  ~steam_cm_client ()
  {
    if (conn_ == nullptr)
      return;

    if (conn_->open)
      shutdown ("client destroyed without disconnecting");

    if (conn_->loops != 0)
    {
      warning ("the Steam connection was destroyed with {} background tasks "
               "still running; this is a launcher bug",
               conn_->loops);

      assert (false && "steam_cm_client destroyed without disconnect()");
    }
  }

  uint64_t steam_cm_client::
  next_job_id () noexcept
  {
    uint64_t r (next_job_++);

    if (r == invalid_job_id)
      r = next_job_++;

    return r;
  }

  asio::awaitable<vector<steam_cm_endpoint>> steam_cm_client::
  discover_endpoints (asio::io_context& ioc,
                      const http_client_traits& ht,
                      uint32_t cell)
  {
    http_client hc (ioc, ht);

    string url (string (cm_directory_url) +
                "?cellid=" + std::to_string (cell) +
                "&cmtype=websockets&format=json");

    trace_l2 ("requesting connection manager list for cell {}", cell);

    http_response r (co_await hc.get (url));

    if (r.status_code () != 200)
      throw steam_protocol_error (
        "the Steam directory returned HTTP " +
        std::to_string (r.status_code ()) +
        " for the connection manager list");

    if (!r.has_body ())
      throw steam_protocol_error (
        "the Steam directory returned an empty connection manager list");

    vector<steam_cm_endpoint> es;

    try
    {
      json::value v (json::parse (*r.body));

      const json::array& l (
        v.as_object ().at ("response").as_object ().at ("serverlist").as_array ());

      es.reserve (l.size ());

      for (const json::value& e : l)
      {
        const json::object& o (e.as_object ());

        auto str ([&o] (const char* k) -> string
        {
          auto i (o.find (k));

          return i != o.end () && i->value ().is_string ()
                   ? string (i->value ().as_string ())
                   : string ();
        });

        steam_cm_endpoint p;

        if (!parse_endpoint (str ("endpoint"), p.host, p.port))
        {
          warning ("ignoring malformed connection manager endpoint '{}'",
                   str ("endpoint"));
          continue;
        }

        p.realm = str ("realm");

        if (auto i (o.find ("load")); i != o.end () && i->value ().is_int64 ())
          p.load = static_cast<int32_t> (i->value ().as_int64 ());

        if (auto i (o.find ("wtd_load")); i != o.end ())
        {
          const json::value& w (i->value ());

          if (w.is_double ())
            p.weighted_load = w.as_double ();
          else if (w.is_int64 ())
            p.weighted_load = static_cast<double> (w.as_int64 ());
        }

        if (!p.realm.empty () && p.realm != public_realm)
          continue;

        es.push_back (move (p));
      }
    }
    catch (const steam_protocol_error&)
    {
      throw;
    }
    catch (const exception& e)
    {
      throw steam_protocol_error (
        string ("failed to parse the connection manager list: ") + e.what ());
    }

    if (es.empty ())
      throw steam_protocol_error (
        "the Steam directory returned no usable websocket connection "
        "managers");

    ranges::stable_sort (es,
                         [] (const steam_cm_endpoint& a,
                             const steam_cm_endpoint& b)
    {
      return a.weighted_load < b.weighted_load;
    });

    trace_l2 ("discovered {} connection managers", es.size ());

    co_return es;
  }

  asio::awaitable<void> steam_cm_client::
  connect (const steam_cm_endpoint& e)
  {
    if (connected ())
      throw steam_protocol_error ("already connected to a connection "
                                  "manager");

    auto c (make_shared<connection> (ioc_, ssl_ctx_));

    if (!SSL_set_tlsext_host_name (c->ws.next_layer ().native_handle (),
                                   e.host.c_str ()))
      throw steam_protocol_error (
        "failed to set the TLS server name for " + e.host);

    tcp::resolver rs (ioc_);

    auto eps (co_await rs.async_resolve (e.host,
                                         std::to_string (e.port),
                                         asio::use_awaitable));

    beast::get_lowest_layer (c->ws).expires_after (traits_.connect_timeout);

    co_await beast::get_lowest_layer (c->ws).async_connect (
      eps, asio::use_awaitable);

    co_await c->ws.next_layer ().async_handshake (ssl::stream_base::client,
                                                  asio::use_awaitable);

    beast::get_lowest_layer (c->ws).expires_never ();

    c->ws.set_option (websocket::stream_base::timeout::suggested (
      beast::role_type::client));

    c->ws.binary (true);
    c->ws.read_message_max (traits_.max_message_size);

    co_await c->ws.async_handshake (e.host + ":" + std::to_string (e.port),
                                    cm_websocket_path,
                                    asio::use_awaitable);

    c->open = true;

    conn_     = c;
    endpoint_ = e;

    steam_id_.reset ();
    session_id_.reset ();
    licenses_.clear ();
    licenses_received_ = false;
    logon_response_.reset ();

    info ("connected to connection manager {}", e.to_string ());

    spawn_loop (c, read_loop (c));
    spawn_loop (c, write_loop (c));

    pb::writer h;
    h.add_uint32 (hello_protocol_version, client_protocol_version);

    co_await send (steam_emsg::client_hello, h.release ());
  }

  asio::awaitable<void> steam_cm_client::
  connect_any (uint32_t cell)
  {
    http_client_traits ht;

    ht.verify_ssl = traits_.verify_ssl;
    ht.proxy_url  = traits_.proxy_url;

    vector<steam_cm_endpoint> es (
      co_await discover_endpoints (ioc_, ht, cell));

    size_t n (min (es.size (), traits_.max_connect_attempts));

    string last;

    for (size_t i (0); i != n; ++i)
    {
      string failed;

      try
      {
        co_await connect (es[i]);
      }
      catch (const exception& ex)
      {
        failed = ex.what ();
      }

      if (failed.empty ())
        co_return;

      last = failed;

      warning ("connection manager {} did not accept us: {}",
               es[i].to_string (),
               last);

      if (conn_ != nullptr)
        co_await disconnect ();
    }

    throw steam_protocol_error (
      "unable to reach any of the " + std::to_string (n) +
      " connection managers tried; last error: " +
      (last.empty () ? "none reported" : last));
  }

  void steam_cm_client::
  spawn_loop (shared_ptr<connection> c, asio::awaitable<void> l)
  {
    ++c->loops;

    asio::co_spawn (ioc_, move (l), [c] (exception_ptr ep)
    {
      if (ep)
      {
        try
        {
          rethrow_exception (ep);
        }
        catch (const exception& e)
        {
          warning ("a connection manager background task ended with an "
                   "error: {}",
                   e.what ());
        }
        catch (...)
        {
          warning ("a connection manager background task ended with an "
                   "unknown error");
        }
      }

      assert (c->loops != 0 && "background loop accounting underflow");

      if (--c->loops == 0)
        c->loops_event.cancel ();
    });
  }

  asio::awaitable<void> steam_cm_client::
  join_loops (shared_ptr<connection> c)
  {
    while (c->loops != 0)
    {
      beast::error_code ec;

      co_await c->loops_event.async_wait (
        asio::redirect_error (asio::use_awaitable, ec));

      c->loops_event.expires_at (asio::steady_timer::time_point::max ());
    }

    co_return;
  }

  void steam_cm_client::
  shutdown (const string& reason)
  {
    auto c (conn_);

    if (c == nullptr)
      return;

    if (c->open)
    {
      c->open         = false;
      c->close_reason = reason;
    }

    beast::error_code ec;
    beast::get_lowest_layer (c->ws).socket ().close (ec);

    c->outbox_event.cancel ();
    c->heartbeat.cancel ();
  }

  void steam_cm_client::
  abandon (const string& reason)
  {
    auto js (move (jobs_));
    jobs_.clear ();

    for (auto& [id, j] : js)
    {
      if (j->response)
        continue;

      j->error = make_exception_ptr (steam_protocol_error (
        "the connection to the Steam connection manager was lost while "
        "waiting for a reply (" + reason + ")"));

      j->event.cancel ();
    }

    session_id_.reset ();

    licenses_event_.cancel ();
    logon_event_.cancel ();
  }

  asio::awaitable<void> steam_cm_client::
  disconnect ()
  {
    if (conn_ == nullptr)
      co_return;

    auto c (conn_);

    if (c->open)
    {
      c->open = false;

      beast::error_code ec;

      co_await c->ws.async_close (websocket::close_code::normal,
                                  asio::redirect_error (asio::use_awaitable,
                                                        ec));

      if (ec)
        trace_l2 ("websocket close returned {}", ec.message ());
    }

    shutdown ("disconnected by the launcher");
    abandon ("disconnected by the launcher");

    co_await join_loops (c);

    conn_.reset ();

    steam_id_.reset ();
    session_id_.reset ();

    info ("disconnected from connection manager {}", endpoint_.to_string ());
  }

  void steam_cm_client::
  enqueue (pb::byte_buffer p)
  {
    assert (conn_ != nullptr && "enqueue on a disconnected client");

    conn_->outbox.push_back (move (p));
    conn_->outbox_event.cancel ();
  }

  void steam_cm_client::
  stamp (steam_message& m) const
  {
    if (steam_id_)
      m.header.steam_id = *steam_id_;

    if (session_id_)
      m.header.session_id = *session_id_;
  }

  asio::awaitable<void> steam_cm_client::
  send (steam_emsg e, pb::byte_buffer b)
  {
    if (!connected ())
      throw steam_protocol_error (
        "cannot send " + to_string (e) +
        ": not connected to a connection manager");

    steam_message m (e, move (b));
    stamp (m);

    trace_l3 ("-> {} ({} byte body)", to_string (e), m.body.size ());

    enqueue (m.encode ());

    co_return;
  }

  asio::awaitable<steam_message> steam_cm_client::
  send_job (steam_emsg e, pb::byte_buffer b)
  {
    if (!connected ())
      throw steam_protocol_error (
        "cannot send " + to_string (e) +
        ": not connected to a connection manager");

    uint64_t id (next_job_id ());

    steam_message m (e, move (b));
    stamp (m);
    m.header.job_id_source = id;

    auto j (make_shared<pending_job> (ioc_));
    j->event.expires_after (traits_.request_timeout);

    jobs_.emplace (id, j);

    trace_l3 ("-> {} (job {}, {} byte body)",
              to_string (e),
              id,
              m.body.size ());

    enqueue (m.encode ());

    beast::error_code ec;

    co_await j->event.async_wait (
      asio::redirect_error (asio::use_awaitable, ec));

    jobs_.erase (id);

    if (j->error)
      rethrow_exception (j->error);

    if (!j->response)
      throw steam_protocol_error (
        "timed out after " +
        std::to_string (traits_.request_timeout.count ()) +
        " ms waiting for a reply to " + to_string (e) +
        " (job " + std::to_string (id) + ")");

    co_return move (*j->response);
  }

  asio::awaitable<pb::byte_buffer> steam_cm_client::
  call_service (const string& method, pb::byte_buffer request)
  {
    assert (!method.empty () && "service method name must not be empty");

    steam_emsg e (session_id_
                    ? steam_emsg::service_method_call_from_client
                    : steam_emsg::service_method_call_from_client_non_authed);

    if (!connected ())
      throw steam_protocol_error ("cannot call " + method +
                                  ": not connected to a connection manager");

    uint64_t id (next_job_id ());

    steam_message m (e, move (request));
    stamp (m);

    m.header.job_id_source   = id;
    m.header.target_job_name = method;

    auto j (make_shared<pending_job> (ioc_));
    j->event.expires_after (traits_.request_timeout);

    jobs_.emplace (id, j);

    trace_l3 ("-> {} (job {}, {} byte request)",
              method,
              id,
              m.body.size ());

    enqueue (m.encode ());

    beast::error_code ec;

    co_await j->event.async_wait (
      asio::redirect_error (asio::use_awaitable, ec));

    jobs_.erase (id);

    if (j->error)
      rethrow_exception (j->error);

    if (!j->response)
      throw steam_protocol_error (
        "timed out after " +
        std::to_string (traits_.request_timeout.count ()) +
        " ms waiting for a response to " + method);

    steam_message r (move (*j->response));

    steam_result rr (r.header.result_or_ok ());

    if (rr != steam_result::ok)
    {
      string c ("call " + method);

      if (r.header.error_message && !r.header.error_message->empty ())
        c += " (" + *r.header.error_message + ")";

      throw steam_exception (rr, c);
    }

    co_return move (r.body);
  }

  asio::awaitable<void> steam_cm_client::
  log_on (uint64_t sid, const string& token, uint32_t cell)
  {
    if (!connected ())
      throw steam_protocol_error ("cannot log on: not connected to a "
                                  "connection manager");

    if (token.empty ())
      throw steam_protocol_error ("cannot log on without a refresh token");

    steam_id_ = sid;

    pb::writer w;

    w.add_uint32 (logon_protocol_version, client_protocol_version);
    w.add_uint32 (logon_cell_id, cell);
    w.add_uint32 (logon_client_package_version, client_package_version);
    w.add_string (logon_client_language, "english");
    w.add_uint32 (logon_client_os_type, client_os_type_windows_10);
    w.add_bool (logon_should_remember_pass, false);
    w.add_fixed64 (logon_client_supplied_id, sid);

    w.add_bool (logon_supports_rate_limit, true);
    w.add_string (logon_access_token, token);

    trace_l2 ("logging on as {}", sid);

    logon_response_.reset ();
    logon_event_.expires_after (traits_.request_timeout);

    co_await send (steam_emsg::client_logon, w.release ());

    beast::error_code ec;

    co_await logon_event_.async_wait (
      asio::redirect_error (asio::use_awaitable, ec));

    logon_event_.expires_at (asio::steady_timer::time_point::max ());

    if (!logon_response_)
    {
      steam_id_.reset ();

      if (ec == asio::error::operation_aborted)
        throw steam_protocol_error (
          "the connection to the Steam connection manager was lost while "
          "logging on" +
          (conn_ != nullptr && !conn_->close_reason.empty ()
             ? " (" + conn_->close_reason + ")"
             : string ()));

      throw steam_protocol_error (
        "timed out after " +
        std::to_string (traits_.request_timeout.count ()) +
        " ms waiting for the connection manager to answer the logon");
    }

    steam_message r (move (*logon_response_));
    logon_response_.reset ();

    steam_result rr (steam_result::fail);
    uint32_t     hb (0);

    try
    {
      pb::reader rd (pb::byte_view (r.body.data (), r.body.size ()));

      while (rd.next ())
      {
        switch (rd.field_number ())
        {
        case logon_response_eresult:
          rr = static_cast<steam_result> (rd.read_int32 ());
          break;

        case logon_response_out_of_game_heartbeat:
          hb = static_cast<uint32_t> (rd.read_int32 ());
          break;

        default:
          rd.skip ();
          break;
        }
      }
    }
    catch (const pb::decode_error& e)
    {
      throw steam_protocol_error (string ("malformed logon response: ") +
                                  e.what ());
    }

    if (rr != steam_result::ok)
    {
      steam_id_.reset ();

      throw steam_exception (rr, "log on to Steam");
    }

    if (!r.header.session_id)
      throw steam_protocol_error (
        "the connection manager accepted the logon but did not assign a "
        "session id");

    session_id_ = *r.header.session_id;

    if (r.header.steam_id)
      steam_id_ = *r.header.steam_id;

    if (hb > 0 && hb <= 300)
      traits_.heartbeat_interval = chrono::seconds (hb);

    info ("logged on as {} (session {}, heartbeat every {}s)",
          *steam_id_,
          *session_id_,
          traits_.heartbeat_interval.count ());

    spawn_loop (conn_, heartbeat_loop (conn_));
  }

  asio::awaitable<bool> steam_cm_client::
  await_licenses (chrono::milliseconds t)
  {
    if (licenses_received_)
      co_return true;

    if (!connected ())
      co_return false;

    licenses_event_.expires_after (t);

    beast::error_code ec;

    co_await licenses_event_.async_wait (
      asio::redirect_error (asio::use_awaitable, ec));

    licenses_event_.expires_at (asio::steady_timer::time_point::max ());

    co_return licenses_received_;
  }

  asio::awaitable<void> steam_cm_client::
  write_loop (shared_ptr<connection> c)
  {
    for (;;)
    {
      if (!c->open)
        break;

      if (c->outbox.empty ())
      {
        beast::error_code ec;

        co_await c->outbox_event.async_wait (
          asio::redirect_error (asio::use_awaitable, ec));

        c->outbox_event.expires_at (asio::steady_timer::time_point::max ());

        continue;
      }

      pb::byte_buffer p (move (c->outbox.front ()));
      c->outbox.pop_front ();

      beast::error_code ec;

      co_await c->ws.async_write (
        asio::buffer (p.data (), p.size ()),
        asio::redirect_error (asio::use_awaitable, ec));

      if (ec)
      {
        if (c->open)
        {
          warning ("failed to send to connection manager {}: {}",
                   endpoint_.to_string (),
                   ec.message ());

          shutdown ("write failed: " + ec.message ());
          abandon ("write failed: " + ec.message ());
        }

        break;
      }
    }

    trace_l3 ("write loop finished");

    co_return;
  }

  asio::awaitable<void> steam_cm_client::
  read_loop (shared_ptr<connection> c)
  {
    beast::flat_buffer b;

    for (;;)
    {
      beast::error_code ec;

      size_t n (co_await c->ws.async_read (
        b, asio::redirect_error (asio::use_awaitable, ec)));

      if (ec)
      {
        if (c->open)
        {
          string r (ec == websocket::error::closed
                      ? "the connection manager closed the connection"
                      : "read failed: " + ec.message ());

          warning ("connection to {} lost: {}", endpoint_.to_string (), r);

          shutdown (r);
          abandon (r);
        }

        break;
      }

      auto d (b.cdata ());

      try
      {
        dispatch (pb::byte_view (
          static_cast<const uint8_t*> (d.data ()), d.size ()));
      }
      catch (const exception& ex)
      {
        warning ("discarding an undecodable {} byte packet: {}",
                 n,
                 ex.what ());
      }

      b.consume (b.size ());
    }

    trace_l3 ("read loop finished");

    co_return;
  }

  asio::awaitable<void> steam_cm_client::
  heartbeat_loop (shared_ptr<connection> c)
  {
    while (c->open && session_id_)
    {
      c->heartbeat.expires_after (traits_.heartbeat_interval);

      beast::error_code ec;

      co_await c->heartbeat.async_wait (
        asio::redirect_error (asio::use_awaitable, ec));

      if (ec || !c->open || !session_id_)
        break;

      steam_message m (steam_emsg::client_heart_beat, pb::byte_buffer ());
      stamp (m);

      c->outbox.push_back (m.encode ());
      c->outbox_event.cancel ();
    }

    trace_l3 ("heartbeat loop finished");

    co_return;
  }

  void steam_cm_client::
  dispatch (pb::byte_view p)
  {
    steam_emsg e (steam_message::peek (p));

    if (e == steam_emsg::multi)
    {
      steam_message m (steam_message::decode (p));

      steam_multi_body mb (steam_multi_body::decode (
        pb::byte_view (m.body.data (), m.body.size ())));

      pb::byte_buffer inflated;
      pb::byte_view   payload (mb.payload.data (), mb.payload.size ());

      if (mb.size_unzipped != 0)
      {
        inflated = compression::gzip_decompress (payload, mb.size_unzipped);
        payload  = pb::byte_view (inflated.data (), inflated.size ());
      }

      for (pb::byte_view s : split_multi_payload (payload))
      {
        try
        {
          dispatch (s);
        }
        catch (const exception& ex)
        {
          warning ("discarding an undecodable packet inside a multi batch: "
                   "{}",
                   ex.what ());
        }
      }

      return;
    }

    dispatch_message (steam_message::decode (p));
  }

  void steam_cm_client::
  dispatch_message (steam_message&& m)
  {
    trace_l3 ("<- {} ({} byte body)", to_string (m.msg), m.body.size ());

    if (m.msg == steam_emsg::client_log_on_response)
    {
      logon_response_ = move (m);
      logon_event_.cancel ();

      return;
    }

    if (m.header.job_id_target)
    {
      auto i (jobs_.find (*m.header.job_id_target));

      if (i != jobs_.end ())
      {
        i->second->response = move (m);
        i->second->event.cancel ();

        return;
      }

      trace_l2 ("ignoring a reply for unknown job {}",
                *m.header.job_id_target);

      return;
    }

    switch (m.msg)
    {
    case steam_emsg::client_logged_off:
      {
        steam_result r (steam_result::fail);

        try
        {
          pb::reader rd (pb::byte_view (m.body.data (), m.body.size ()));

          while (rd.next ())
          {
            if (rd.field_number () == logged_off_eresult)
              r = static_cast<steam_result> (rd.read_int32 ());
            else
              rd.skip ();
          }
        }
        catch (const pb::decode_error&)
        {
        }

        warning ("Steam logged us off: {}", describe (r));

        session_id_.reset ();

        shutdown ("logged off by Steam: " + to_string (r));
        abandon ("logged off by Steam: " + to_string (r));

        break;
      }

    case steam_emsg::client_license_list:
      {
        licenses_.clear ();

        try
        {
          pb::reader rd (pb::byte_view (m.body.data (), m.body.size ()));

          while (rd.next ())
          {
            if (rd.field_number () != license_list_licenses)
            {
              rd.skip ();
              continue;
            }

            pb::reader l (rd.read_message ());

            while (l.next ())
            {
              if (l.field_number () == license_package_id)
                licenses_.push_back (l.read_uint32 ());
              else
                l.skip ();
            }
          }
        }
        catch (const pb::decode_error& e)
        {
          warning ("failed to parse the license list: {}", e.what ());
        }

        licenses_received_ = true;
        licenses_event_.cancel ();

        trace_l2 ("account holds {} licenses", licenses_.size ());

        break;
      }

    default:

      trace_l3 ("ignoring unsolicited {}", to_string (m.msg));
      break;
    }
  }
}
