#pragma once

#include <launcher/protobuf/protobuf-wire.hxx>
#include <launcher/steam/steam-cm-client.hxx>
#include <launcher/steam/steam-result.hxx>

#include <boost/asio/awaitable.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace launcher
{
  enum class steam_guard_type : std::int32_t
  {
    unknown             = 0,
    none                = 1,

    email_code          = 2,

    device_code         = 3,

    device_confirmation = 4,

    email_confirmation  = 5,

    machine_token       = 6,

    legacy_machine_auth = 7
  };

  std::string
  to_string (steam_guard_type);

  bool
  requires_code (steam_guard_type) noexcept;

  struct steam_guard_challenge
  {
    steam_guard_type type = steam_guard_type::unknown;

    std::string associated_message;
  };

  struct steam_auth_tokens
  {
    std::uint64_t steam_id = 0;
    std::string   account_name;

    std::string refresh_token;

    std::string access_token;

    std::string guard_data;

    bool
    empty () const noexcept
    {
      return refresh_token.empty ();
    }
  };

  struct steam_auth_prompt
  {
    std::function<asio::awaitable<std::string> (const steam_guard_challenge&)>
      request_guard_code;

    std::function<void (const steam_guard_challenge&)>
      announce_device_confirmation;

    std::function<void (const std::string& url)>
      present_qr_challenge;

    std::function<void ()>
      poll_tick;
  };

  std::uint64_t
  steam_id_from_token (const std::string& token);

  class steam_authenticator
  {
  public:
    steam_authenticator (asio::io_context&, steam_cm_client&);

    steam_authenticator (const steam_authenticator&) = delete;
    steam_authenticator& operator= (const steam_authenticator&) = delete;

    void
    set_device_name (std::string);

    asio::awaitable<steam_auth_tokens>
    log_in_with_credentials (const std::string& account_name,
                             const std::string& password,
                             const std::string& guard_data,
                             const steam_auth_prompt&);

    asio::awaitable<steam_auth_tokens>
    log_in_with_qr (const steam_auth_prompt&);

  private:
    struct password_key
    {
      std::string   modulus_hex;
      std::string   exponent_hex;
      std::uint64_t timestamp = 0;
    };

    asio::awaitable<password_key>
    fetch_password_key (const std::string& account_name);

    struct session
    {
      std::uint64_t                      client_id = 0;
      pb::byte_buffer                    request_id;
      std::chrono::milliseconds          interval {5000};
      std::vector<steam_guard_challenge> allowed;
      std::uint64_t                      steam_id = 0;
      std::string                        challenge_url;
      std::string                        extended_error;
    };

    asio::awaitable<steam_auth_tokens>
    poll_until_complete (session&,
                         const steam_auth_prompt&,
                         const std::function<void (const std::string&)>&
                           on_new_challenge);

    asio::awaitable<void>
    submit_guard_code (const session&,
                       const std::string& code,
                       steam_guard_type);

    pb::byte_buffer
    device_details () const;

  private:
    asio::io_context& ioc_;
    steam_cm_client&  cm_;
    std::string       device_name_;
  };
}
