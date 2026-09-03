#include <launcher/steam/steam-auth.hxx>

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <cassert>

#include <boost/json.hpp>

#include <launcher/launcher-log.hxx>
#include <launcher/steam/steam-crypto.hxx>

using namespace std;
using namespace std::chrono_literals;

namespace launcher
{
  namespace
  {
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

    constexpr const char* method_rsa_key =
      "Authentication.GetPasswordRSAPublicKey#1";
    constexpr const char* method_begin_credentials =
      "Authentication.BeginAuthSessionViaCredentials#1";
    constexpr const char* method_begin_qr =
      "Authentication.BeginAuthSessionViaQR#1";
    constexpr const char* method_update_guard =
      "Authentication.UpdateAuthSessionWithSteamGuardCode#1";
    constexpr const char* method_poll =
      "Authentication.PollAuthSessionStatus#1";

    constexpr uint32_t platform_steam_client = 1;

    constexpr int32_t persistence_persistent = 1;

    constexpr const char* website_id_client = "Client";

    constexpr int32_t os_type_windows_10 = 16;

    constexpr chrono::milliseconds min_poll_interval (1000);
    constexpr chrono::milliseconds max_poll_interval (30000);
    constexpr chrono::milliseconds default_poll_interval (5000);

    constexpr chrono::minutes poll_deadline (5);

    enum : uint32_t
    {
      rsa_request_account_name = 1,

      rsa_response_modulus   = 1,
      rsa_response_exponent  = 2,
      rsa_response_timestamp = 3
    };

    enum : uint32_t
    {
      device_friendly_name = 1,
      device_platform_type = 2,
      device_os_type       = 3
    };

    enum : uint32_t
    {
      begin_device_friendly_name  = 1,
      begin_account_name          = 2,
      begin_encrypted_password    = 3,
      begin_encryption_timestamp  = 4,
      begin_remember_login        = 5,
      begin_platform_type         = 6,
      begin_persistence           = 7,
      begin_website_id            = 8,
      begin_device_details        = 9,
      begin_guard_data            = 10
    };

    enum : uint32_t
    {
      qr_device_friendly_name = 1,
      qr_platform_type        = 2,
      qr_device_details       = 3,
      qr_website_id           = 4
    };

    enum : uint32_t
    {
      begin_response_client_id             = 1,
      begin_response_request_id            = 2,
      begin_response_interval              = 3,
      begin_response_allowed_confirmations = 4,
      begin_response_steam_id              = 5,
      begin_response_extended_error        = 8
    };

    enum : uint32_t
    {
      qr_response_client_id             = 1,
      qr_response_challenge_url         = 2,
      qr_response_request_id            = 3,
      qr_response_interval              = 4,
      qr_response_allowed_confirmations = 5
    };

    enum : uint32_t
    {
      confirmation_type    = 1,
      confirmation_message = 2
    };

    enum : uint32_t
    {
      guard_client_id = 1,
      guard_steam_id  = 2,
      guard_code      = 3,
      guard_code_type = 4
    };

    enum : uint32_t
    {
      poll_request_client_id  = 1,
      poll_request_request_id = 2,

      poll_response_new_client_id     = 1,
      poll_response_new_challenge_url = 2,
      poll_response_refresh_token     = 3,
      poll_response_access_token      = 4,
      poll_response_account_name      = 6,
      poll_response_new_guard_data    = 7
    };

    chrono::milliseconds
    interval_from (float s)
    {
      if (!(s > 0.0f) || s > 3600.0f)
        return default_poll_interval;

      auto r (chrono::milliseconds (
        static_cast<int64_t> (s * 1000.0f)));

      return clamp (r, min_poll_interval, max_poll_interval);
    }

    void
    read_confirmations (pb::reader& r,
                        vector<steam_guard_challenge>& out)
    {
      pb::reader c (r.read_message ());

      steam_guard_challenge g;

      while (c.next ())
      {
        switch (c.field_number ())
        {
        case confirmation_type:
          g.type = static_cast<steam_guard_type> (c.read_int32 ());
          break;

        case confirmation_message:
          g.associated_message = string (c.read_string ());
          break;

        default:
          c.skip ();
          break;
        }
      }

      out.push_back (move (g));
    }
  }

  uint64_t
  steam_id_from_token (const string& t)
  {
    size_t a (t.find ('.'));

    if (a == string::npos)
      throw steam_protocol_error ("the Steam token is not a JWT");

    size_t b (t.find ('.', a + 1));

    if (b == string::npos)
      throw steam_protocol_error ("the Steam token is not a JWT");

    string p (t.substr (a + 1, b - a - 1));

    if (p.empty ())
      throw steam_protocol_error ("the Steam token has an empty payload");

    for (char& c : p)
    {
      if (c == '-')
        c = '+';
      else if (c == '_')
        c = '/';
    }

    p.append ((4 - p.size () % 4) % 4, '=');

    string j;

    try
    {
      pb::byte_buffer d (crypto::base64_decode (p));
      j.assign (d.begin (), d.end ());
    }
    catch (const crypto::crypto_error& e)
    {
      throw steam_protocol_error (
        string ("the Steam token payload is not valid base64: ") + e.what ());
    }

    try
    {
      boost::json::value v (boost::json::parse (j));

      const boost::json::value& sub (v.as_object ().at ("sub"));

      if (sub.is_string ())
      {
        const boost::json::string& t2 (sub.as_string ());

        size_t   n (0);
        uint64_t r (stoull (string (t2), &n));

        if (n != t2.size ())
          throw steam_protocol_error (
            "the Steam token subject is not a Steam ID");

        return r;
      }

      if (sub.is_uint64 ())
        return sub.as_uint64 ();

      if (sub.is_int64 () && sub.as_int64 () >= 0)
        return static_cast<uint64_t> (sub.as_int64 ());
    }
    catch (const steam_protocol_error&)
    {
      throw;
    }
    catch (const exception& e)
    {
      throw steam_protocol_error (
        string ("failed to read the Steam token payload: ") + e.what ());
    }

    throw steam_protocol_error (
      "the Steam token does not identify an account");
  }

  string
  to_string (steam_guard_type t)
  {
    switch (t)
    {
    case steam_guard_type::unknown:             return "unknown";
    case steam_guard_type::none:                return "none";
    case steam_guard_type::email_code:          return "email code";
    case steam_guard_type::device_code:         return "device code";
    case steam_guard_type::device_confirmation: return "device confirmation";
    case steam_guard_type::email_confirmation:  return "email confirmation";
    case steam_guard_type::machine_token:       return "machine token";
    case steam_guard_type::legacy_machine_auth: return "legacy machine auth";
    }

    return "EAuthSessionGuardType(" +
           std::to_string (static_cast<int32_t> (t)) + ")";
  }

  bool
  requires_code (steam_guard_type t) noexcept
  {
    return t == steam_guard_type::email_code ||
           t == steam_guard_type::device_code;
  }

  steam_authenticator::
  steam_authenticator (asio::io_context& i, steam_cm_client& c)
    : ioc_ (i),
      cm_ (c),
      device_name_ ("iw4x-launcher")
  {
  }

  void steam_authenticator::
  set_device_name (string n)
  {
    if (!n.empty ())
      device_name_ = move (n);
  }

  pb::byte_buffer steam_authenticator::
  device_details () const
  {
    pb::writer w;

    w.add_string (device_friendly_name, device_name_);
    w.add_uint32 (device_platform_type, platform_steam_client);
    w.add_int32 (device_os_type, os_type_windows_10);

    return w.release ();
  }

  asio::awaitable<steam_authenticator::password_key> steam_authenticator::
  fetch_password_key (const string& account)
  {
    pb::writer w;
    w.add_string (rsa_request_account_name, account);

    pb::byte_buffer r (
      co_await cm_.call_service (method_rsa_key, w.release ()));

    password_key k;

    pb::reader rd (pb::byte_view (r.data (), r.size ()));

    while (rd.next ())
    {
      switch (rd.field_number ())
      {
      case rsa_response_modulus:
        k.modulus_hex = string (rd.read_string ());
        break;

      case rsa_response_exponent:
        k.exponent_hex = string (rd.read_string ());
        break;

      case rsa_response_timestamp:
        k.timestamp = rd.read_uint64 ();
        break;

      default:
        rd.skip ();
        break;
      }
    }

    if (k.modulus_hex.empty () || k.exponent_hex.empty ())
      throw steam_protocol_error (
        "Steam returned an incomplete password encryption key for account '" +
        account + "'");

    co_return k;
  }

  asio::awaitable<void> steam_authenticator::
  submit_guard_code (const session& s, const string& code, steam_guard_type t)
  {
    pb::writer w;

    w.add_uint64 (guard_client_id, s.client_id);
    w.add_fixed64 (guard_steam_id, s.steam_id);
    w.add_string (guard_code, code);
    w.add_int32 (guard_code_type, static_cast<int32_t> (t));

    co_await cm_.call_service (method_update_guard, w.release ());
  }

  asio::awaitable<steam_auth_tokens> steam_authenticator::
  poll_until_complete (session& s,
                       const steam_auth_prompt& p,
                       const function<void (const string&)>& on_challenge)
  {
    assert (s.client_id != 0 && "polling an unstarted authentication "
                                "session");

    asio::steady_timer t (ioc_);

    auto deadline (chrono::steady_clock::now () + poll_deadline);

    for (;;)
    {
      if (chrono::steady_clock::now () >= deadline)
        throw steam_protocol_error (
          "the Steam login was not completed within " +
          std::to_string (chrono::duration_cast<chrono::minutes> (
                            poll_deadline).count ()) +
          " minutes");

      t.expires_after (s.interval);

      beast::error_code ec;

      co_await t.async_wait (asio::redirect_error (asio::use_awaitable, ec));

      if (ec)
        throw steam_protocol_error (
          "the Steam login poll was cancelled: " + ec.message ());

      if (p.poll_tick)
        p.poll_tick ();

      pb::writer w;

      w.add_uint64 (poll_request_client_id, s.client_id);
      w.add_bytes (poll_request_request_id,
                   pb::byte_view (s.request_id.data (), s.request_id.size ()));

      pb::byte_buffer r (
        co_await cm_.call_service (method_poll, w.release ()));

      steam_auth_tokens tk;

      uint64_t nc (0);
      string   nu;

      pb::reader rd (pb::byte_view (r.data (), r.size ()));

      while (rd.next ())
      {
        switch (rd.field_number ())
        {
        case poll_response_new_client_id:
          nc = rd.read_uint64 ();
          break;

        case poll_response_new_challenge_url:
          nu = string (rd.read_string ());
          break;

        case poll_response_refresh_token:
          tk.refresh_token = string (rd.read_string ());
          break;

        case poll_response_access_token:
          tk.access_token = string (rd.read_string ());
          break;

        case poll_response_account_name:
          tk.account_name = string (rd.read_string ());
          break;

        case poll_response_new_guard_data:
          tk.guard_data = string (rd.read_string ());
          break;

        default:
          rd.skip ();
          break;
        }
      }

      if (nc != 0 && nc != s.client_id)
      {
        trace_l2 ("authentication session client id rotated");
        s.client_id = nc;
      }

      if (!nu.empty () && nu != s.challenge_url)
      {
        s.challenge_url = nu;

        if (on_challenge)
          on_challenge (nu);
      }

      if (!tk.refresh_token.empty ())
      {
        uint64_t ts (steam_id_from_token (tk.refresh_token));

        if (s.steam_id != 0 && ts != s.steam_id)
          throw steam_protocol_error (
            "Steam issued a token for account " + std::to_string (ts) +
            " but the login session was started for " +
            std::to_string (s.steam_id));

        tk.steam_id = ts;

        co_return tk;
      }
    }
  }

  asio::awaitable<steam_auth_tokens> steam_authenticator::
  log_in_with_credentials (const string& account,
                           const string& password,
                           const string& guard,
                           const steam_auth_prompt& p)
  {
    if (account.empty ())
      throw steam_protocol_error ("an account name is required to log in");

    if (password.empty ())
      throw steam_protocol_error ("a password is required to log in");

    password_key k (co_await fetch_password_key (account));

    string encrypted;

    {
      pb::byte_buffer c (crypto::rsa_encrypt_pkcs1 (
        k.modulus_hex,
        k.exponent_hex,
        pb::byte_view (
          reinterpret_cast<const uint8_t*> (password.data ()),
          password.size ())));

      encrypted = crypto::base64_encode (
        pb::byte_view (c.data (), c.size ()));

      crypto::secure_zero (span<uint8_t> (c.data (), c.size ()));
    }

    pb::writer w;

    w.add_string (begin_device_friendly_name, device_name_);
    w.add_string (begin_account_name, account);
    w.add_string (begin_encrypted_password, encrypted);
    w.add_uint64 (begin_encryption_timestamp, k.timestamp);
    w.add_bool (begin_remember_login, true);
    w.add_uint32 (begin_platform_type, platform_steam_client);
    w.add_int32 (begin_persistence, persistence_persistent);
    w.add_string (begin_website_id, website_id_client);

    {
      pb::byte_buffer d (device_details ());
      w.add_message (begin_device_details,
                     pb::byte_view (d.data (), d.size ()));
    }

    if (!guard.empty ())
      w.add_string (begin_guard_data, guard);

    crypto::secure_zero (encrypted);

    trace_l2 ("beginning a credential login for account '{}'", account);

    pb::byte_buffer r (
      co_await cm_.call_service (method_begin_credentials, w.release ()));

    session s;
    s.interval = default_poll_interval;

    pb::reader rd (pb::byte_view (r.data (), r.size ()));

    while (rd.next ())
    {
      switch (rd.field_number ())
      {
      case begin_response_client_id:
        s.client_id = rd.read_uint64 ();
        break;

      case begin_response_request_id:
        {
          pb::byte_view b (rd.read_bytes ());
          s.request_id.assign (b.begin (), b.end ());
          break;
        }

      case begin_response_interval:
        s.interval = interval_from (rd.read_float ());
        break;

      case begin_response_allowed_confirmations:
        read_confirmations (rd, s.allowed);
        break;

      case begin_response_steam_id:
        s.steam_id = rd.read_uint64 ();
        break;

      case begin_response_extended_error:
        s.extended_error = string (rd.read_string ());
        break;

      default:
        rd.skip ();
        break;
      }
    }

    if (s.client_id == 0 || s.request_id.empty ())
      throw steam_protocol_error (
        "Steam did not start a login session" +
        (s.extended_error.empty () ? string ()
                                   : " (" + s.extended_error + ")"));

    const steam_guard_challenge* code_challenge (nullptr);
    const steam_guard_challenge* push_challenge (nullptr);

    for (const steam_guard_challenge& g : s.allowed)
    {
      if (requires_code (g.type) && code_challenge == nullptr)
        code_challenge = &g;
      else if ((g.type == steam_guard_type::device_confirmation ||
                g.type == steam_guard_type::email_confirmation) &&
               push_challenge == nullptr)
        push_challenge = &g;
    }

    if (code_challenge != nullptr)
    {
      if (!p.request_guard_code)
        throw steam_protocol_error (
          "the account requires a Steam Guard " +
          to_string (code_challenge->type) +
          ", but this session cannot prompt for one");

      string c (co_await p.request_guard_code (*code_challenge));

      if (c.empty ())
        throw steam_protocol_error ("the Steam Guard prompt was cancelled");

      co_await submit_guard_code (s, c, code_challenge->type);

      crypto::secure_zero (c);
    }
    else if (push_challenge != nullptr)
    {
      if (p.announce_device_confirmation)
        p.announce_device_confirmation (*push_challenge);
    }

    steam_auth_tokens tk (
      co_await poll_until_complete (s, p, nullptr));

    if (tk.account_name.empty ())
      tk.account_name = account;

    info ("authenticated as '{}' ({})", tk.account_name, tk.steam_id);

    co_return tk;
  }

  asio::awaitable<steam_auth_tokens> steam_authenticator::
  log_in_with_qr (const steam_auth_prompt& p)
  {
    if (!p.present_qr_challenge)
      throw steam_protocol_error (
        "a QR login was requested but this session cannot display a QR code");

    pb::writer w;

    w.add_string (qr_device_friendly_name, device_name_);
    w.add_uint32 (qr_platform_type, platform_steam_client);

    {
      pb::byte_buffer d (device_details ());
      w.add_message (qr_device_details,
                     pb::byte_view (d.data (), d.size ()));
    }

    w.add_string (qr_website_id, website_id_client);

    trace_l2 ("beginning a QR login");

    pb::byte_buffer r (
      co_await cm_.call_service (method_begin_qr, w.release ()));

    session s;
    s.interval = default_poll_interval;

    pb::reader rd (pb::byte_view (r.data (), r.size ()));

    while (rd.next ())
    {
      switch (rd.field_number ())
      {
      case qr_response_client_id:
        s.client_id = rd.read_uint64 ();
        break;

      case qr_response_challenge_url:
        s.challenge_url = string (rd.read_string ());
        break;

      case qr_response_request_id:
        {
          pb::byte_view b (rd.read_bytes ());
          s.request_id.assign (b.begin (), b.end ());
          break;
        }

      case qr_response_interval:
        s.interval = interval_from (rd.read_float ());
        break;

      case qr_response_allowed_confirmations:
        read_confirmations (rd, s.allowed);
        break;

      default:
        rd.skip ();
        break;
      }
    }

    if (s.client_id == 0 || s.request_id.empty () ||
        s.challenge_url.empty ())
      throw steam_protocol_error ("Steam did not start a QR login session");

    p.present_qr_challenge (s.challenge_url);

    steam_auth_tokens tk (co_await poll_until_complete (
      s,
      p,
      [&p] (const string& u) { p.present_qr_challenge (u); }));

    info ("authenticated as '{}' ({}) via QR", tk.account_name, tk.steam_id);

    co_return tk;
  }
}
