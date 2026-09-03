#pragma once

#include <launcher/protobuf/protobuf-wire.hxx>
#include <launcher/steam/steam-result.hxx>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace launcher
{
  enum class steam_emsg : std::uint32_t
  {
    invalid = 0,

    multi = 1,

    service_method_response         = 147,
    service_method_call_from_client = 151,

    client_heart_beat      = 703,
    client_log_on_response = 751,
    client_logged_off      = 757,

    client_license_list = 780,

    client_get_depot_decryption_key          = 5438,
    client_get_depot_decryption_key_response = 5439,

    client_logon = 5514,

    service_method_call_from_client_non_authed = 9804,

    client_hello = 9805
  };

  std::string
  to_string (steam_emsg);

  inline constexpr std::uint32_t emsg_protobuf_flag = 0x80000000u;
  inline constexpr std::uint32_t emsg_mask          = 0x7fffffffu;

  inline constexpr std::uint64_t invalid_job_id =
    0xffffffffffffffffull;

  class steam_protocol_error : public std::runtime_error
  {
  public:
    explicit
    steam_protocol_error (const std::string& what);
  };

  struct steam_message_header
  {
    std::optional<std::uint64_t> steam_id;
    std::optional<std::int32_t>  session_id;
    std::optional<std::uint64_t> job_id_source;
    std::optional<std::uint64_t> job_id_target;
    std::optional<std::string>   target_job_name;
    std::optional<steam_result>  result;
    std::optional<std::string>   error_message;

    pb::byte_buffer
    encode () const;

    static steam_message_header
    decode (pb::byte_view);

    steam_result
    result_or_ok () const noexcept
    {
      return result.value_or (steam_result::ok);
    }
  };

  struct steam_message
  {
    steam_emsg           msg = steam_emsg::invalid;
    steam_message_header header;
    pb::byte_buffer      body;

    steam_message () = default;

    steam_message (steam_emsg m, pb::byte_buffer b)
      : msg (m), body (std::move (b)) {}

    pb::byte_buffer
    encode () const;

    static steam_message
    decode (pb::byte_view);

    static steam_emsg
    peek (pb::byte_view);
  };

  std::vector<pb::byte_view>
  split_multi_payload (pb::byte_view payload);

  struct steam_multi_body
  {
    std::uint32_t   size_unzipped = 0;
    pb::byte_buffer payload;

    static steam_multi_body
    decode (pb::byte_view);
  };
}
