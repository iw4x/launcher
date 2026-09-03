#include <launcher/steam/steam-content.hxx>

#include <algorithm>
#include <cassert>

#include <launcher/launcher-log.hxx>
#include <launcher/steam/steam-message.hxx>

using namespace std;

namespace launcher
{
  namespace
  {
    constexpr auto warning ([] (auto&&... a)
    {
      log::warning (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr auto trace_l2 ([] (auto&&... a)
    {
      log::trace_l2 (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr const char* method_get_servers =
      "ContentServerDirectory.GetServersForSteamPipe#1";
    constexpr const char* method_manifest_code =
      "ContentServerDirectory.GetManifestRequestCode#1";
    constexpr const char* method_cdn_auth_token =
      "ContentServerDirectory.GetCDNAuthToken#1";

    enum : uint32_t
    {
      servers_request_cell_id     = 1,
      servers_request_max_servers = 2
    };

    enum : uint32_t
    {
      server_type            = 1,
      server_load            = 4,
      server_weighted_load   = 5,
      server_host            = 8,
      server_vhost           = 9,
      server_https_support   = 12,
      server_allowed_app_ids = 13
    };

    enum : uint32_t
    {
      servers_response_servers = 1
    };

    enum : uint32_t
    {
      code_request_app_id      = 1,
      code_request_depot_id    = 2,
      code_request_manifest_id = 3,
      code_request_app_branch  = 4,

      code_response_code = 1
    };

    enum : uint32_t
    {
      token_request_depot_id  = 1,
      token_request_host_name = 2,
      token_request_app_id    = 3,

      token_response_token = 1
    };

    enum : uint32_t
    {
      depot_key_request_depot_id = 1,
      depot_key_request_app_id   = 2,

      depot_key_response_eresult  = 1,
      depot_key_response_depot_id = 2,
      depot_key_response_key      = 3
    };
  }

  string steam_content_server::
  base_url () const
  {
    const string& h (vhost.empty () ? host : vhost);

    string r (https ? "https://" : "http://");
    r += h;

    if ((https && port != 443) || (!https && port != 80))
    {
      r += ':';
      r += std::to_string (port);
    }

    return r;
  }

  bool steam_content_server::
  serves (uint32_t a) const noexcept
  {
    if (allowed_app_ids.empty ())
      return true;

    return ranges::find (allowed_app_ids, a) != allowed_app_ids.end ();
  }

  steam_content_client::
  steam_content_client (steam_cm_client& c)
    : cm_ (c)
  {
  }

  asio::awaitable<vector<steam_content_server>> steam_content_client::
  fetch_servers (uint32_t cell, uint32_t app, uint32_t max)
  {
    pb::writer w;

    w.add_uint32 (servers_request_cell_id, cell);
    w.add_uint32 (servers_request_max_servers, max);

    pb::byte_buffer r (
      co_await cm_.call_service (method_get_servers, w.release ()));

    vector<steam_content_server> ss;

    pb::reader rd (pb::byte_view (r.data (), r.size ()));

    while (rd.next ())
    {
      if (rd.field_number () != servers_response_servers)
      {
        rd.skip ();
        continue;
      }

      pb::reader s (rd.read_message ());

      steam_content_server v;
      string               https;

      while (s.next ())
      {
        switch (s.field_number ())
        {
        case server_type:
          v.type = string (s.read_string ());
          break;

        case server_load:
          v.load = s.read_int32 ();
          break;

        case server_weighted_load:
          v.weighted_load = s.read_float ();
          break;

        case server_host:
          v.host = string (s.read_string ());
          break;

        case server_vhost:
          v.vhost = string (s.read_string ());
          break;

        case server_https_support:
          https = string (s.read_string ());
          break;

        case server_allowed_app_ids:

          if (s.type () == pb::wire_type::length_delimited)
            s.read_packed_uint32 (v.allowed_app_ids);
          else
            v.allowed_app_ids.push_back (s.read_uint32 ());
          break;

        default:
          s.skip ();
          break;
        }
      }

      if (v.host.empty () && v.vhost.empty ())
        continue;

      v.https = https == "mandatory" || https == "optional";
      v.port  = v.https ? 443 : 80;

      if (app != 0 && !v.serves (app))
        continue;

      ss.push_back (move (v));
    }

    if (ss.empty ())
      throw steam_protocol_error (
        "Steam returned no content servers for cell " +
        std::to_string (cell) +
        (app != 0 ? " that serve app " + std::to_string (app) : string ()));

    ranges::stable_sort (ss,
                         [] (const steam_content_server& a,
                             const steam_content_server& b)
    {
      return a.weighted_load < b.weighted_load;
    });

    trace_l2 ("discovered {} content servers for cell {}", ss.size (), cell);

    co_return ss;
  }

  asio::awaitable<uint64_t> steam_content_client::
  manifest_request_code (uint32_t app,
                         uint32_t depot,
                         uint64_t manifest,
                         const string& branch)
  {
    assert (app != 0 && depot != 0 && manifest != 0 &&
            "manifest request code needs a fully specified manifest");

    pb::writer w;

    w.add_uint32 (code_request_app_id, app);
    w.add_uint32 (code_request_depot_id, depot);
    w.add_uint64 (code_request_manifest_id, manifest);
    w.add_string (code_request_app_branch, branch);

    pb::byte_buffer r (
      co_await cm_.call_service (method_manifest_code, w.release ()));

    uint64_t c (0);

    pb::reader rd (pb::byte_view (r.data (), r.size ()));

    while (rd.next ())
    {
      if (rd.field_number () == code_response_code)
        c = rd.read_uint64 ();
      else
        rd.skip ();
    }

    if (c == 0)
      throw steam_protocol_error (
        "Steam declined to authorize manifest " + std::to_string (manifest) +
        " of depot " + std::to_string (depot) + " for app " +
        std::to_string (app) +
        "; the account may not own the game, or the manifest may no longer "
        "be available on the '" + branch + "' branch");

    trace_l2 ("obtained manifest request code for depot {} manifest {}",
              depot,
              manifest);

    co_return c;
  }

  asio::awaitable<crypto::aes_key> steam_content_client::
  depot_key (uint32_t app, uint32_t depot)
  {
    assert (app != 0 && depot != 0 && "depot key needs an app and a depot");

    pb::writer w;

    w.add_uint32 (depot_key_request_depot_id, depot);
    w.add_uint32 (depot_key_request_app_id, app);

    steam_message m (co_await cm_.send_job (
      steam_emsg::client_get_depot_decryption_key, w.release ()));

    steam_result   res (steam_result::fail);
    uint32_t       id (0);
    pb::byte_view  k;

    pb::reader rd (pb::byte_view (m.body.data (), m.body.size ()));

    while (rd.next ())
    {
      switch (rd.field_number ())
      {
      case depot_key_response_eresult:
        res = static_cast<steam_result> (rd.read_int32 ());
        break;

      case depot_key_response_depot_id:
        id = rd.read_uint32 ();
        break;

      case depot_key_response_key:
        k = rd.read_bytes ();
        break;

      default:
        rd.skip ();
        break;
      }
    }

    if (res != steam_result::ok)
      throw steam_exception (
        res,
        "obtain the decryption key for depot " + std::to_string (depot) +
          " of app " + std::to_string (app) +
          " (this normally means the account does not own the game)");

    if (id != depot)
      throw steam_protocol_error (
        "Steam returned a decryption key for depot " + std::to_string (id) +
        " but depot " + std::to_string (depot) + " was requested");

    if (k.size () != crypto::aes_key_size)
      throw steam_protocol_error (
        "Steam returned a " + std::to_string (k.size ()) +
        " byte decryption key for depot " + std::to_string (depot) +
        ", expected " + std::to_string (crypto::aes_key_size));

    crypto::aes_key r {};
    ranges::copy (k, r.begin ());

    trace_l2 ("obtained the decryption key for depot {}", depot);

    co_return r;
  }

  asio::awaitable<string> steam_content_client::
  cdn_auth_token (uint32_t app, uint32_t depot, const string& host)
  {
    pb::writer w;

    w.add_uint32 (token_request_depot_id, depot);
    w.add_string (token_request_host_name, host);
    w.add_uint32 (token_request_app_id, app);

    pb::byte_buffer r;

    try
    {
      r = co_await cm_.call_service (method_cdn_auth_token, w.release ());
    }
    catch (const steam_exception& e)
    {
      trace_l2 ("no CDN auth token for {} (depot {}): {}",
                host,
                depot,
                e.what ());

      co_return string ();
    }

    string t;

    pb::reader rd (pb::byte_view (r.data (), r.size ()));

    while (rd.next ())
    {
      if (rd.field_number () == token_response_token)
        t = string (rd.read_string ());
      else
        rd.skip ();
    }

    co_return t;
  }
}
