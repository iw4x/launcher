#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace launcher
{
  enum class steam_result : std::int32_t
  {
    invalid                                      = 0,
    ok                                           = 1,
    fail                                         = 2,
    no_connection                                = 3,
    invalid_password                             = 5,
    logged_in_elsewhere                          = 6,
    invalid_protocol_ver                         = 7,
    invalid_param                                = 8,
    file_not_found                               = 9,
    busy                                         = 10,
    invalid_state                                = 11,
    invalid_name                                 = 12,
    invalid_email                                = 13,
    duplicate_name                               = 14,
    access_denied                                = 15,
    timeout                                      = 16,
    banned                                       = 17,
    account_not_found                            = 18,
    invalid_steam_id                             = 19,
    service_unavailable                          = 20,
    not_logged_on                                = 21,
    pending                                      = 22,
    encryption_failure                           = 23,
    insufficient_privilege                       = 24,
    limit_exceeded                               = 25,
    revoked                                      = 26,
    expired                                      = 27,
    already_redeemed                             = 28,
    duplicate_request                            = 29,
    already_owned                                = 30,
    ip_not_found                                 = 31,
    persist_failed                               = 32,
    locking_failed                               = 33,
    logon_session_replaced                       = 34,
    connect_failed                               = 35,
    handshake_failed                             = 36,
    io_failure                                   = 37,
    remote_disconnect                            = 38,
    shopping_cart_not_found                      = 39,
    blocked                                      = 40,
    ignored                                      = 41,
    no_match                                     = 42,
    account_disabled                             = 43,
    service_read_only                            = 44,
    account_not_featured                         = 45,
    administrator_ok                             = 46,
    content_version                              = 47,
    try_another_cm                               = 48,
    password_required_to_kick_session            = 49,
    already_logged_in_elsewhere                  = 50,
    suspended                                    = 51,
    cancelled                                    = 52,
    data_corruption                              = 53,
    disk_full                                    = 54,
    remote_call_failed                           = 55,
    password_unset                               = 56,
    external_account_unlinked                    = 57,
    psn_ticket_invalid                           = 58,
    external_account_already_linked              = 59,
    remote_file_conflict                         = 60,
    illegal_password                             = 61,
    same_as_previous_value                       = 62,
    account_logon_denied                         = 63,
    cannot_use_old_password                      = 64,
    invalid_login_auth_code                      = 65,
    account_logon_denied_no_mail                 = 66,
    hardware_not_capable_of_ipt                  = 67,
    ipt_init_error                               = 68,
    parental_control_restricted                  = 69,
    facebook_query_error                         = 70,
    expired_login_auth_code                      = 71,
    ip_login_restriction_failed                  = 72,
    account_locked_down                          = 73,
    account_logon_denied_verified_email_required = 74,
    no_matching_url                              = 75,
    bad_response                                 = 76,
    require_password_re_entry                    = 77,
    value_out_of_range                           = 78,
    unexpected_error                             = 79,
    disabled                                     = 80,
    invalid_ceg_submission                       = 81,
    restricted_device                            = 82,
    region_locked                                = 83,
    rate_limit_exceeded                          = 84,
    account_login_denied_need_two_factor         = 85,
    item_deleted                                 = 86,
    account_login_denied_throttle                = 87,
    two_factor_code_mismatch                     = 88,
    two_factor_activation_code_mismatch          = 89,
    account_associated_to_multiple_partners      = 90,
    not_modified                                 = 91,
    no_mobile_device                             = 92,
    time_not_synced                              = 93,
    sms_code_failed                              = 94,
    account_limit_exceeded                       = 95,
    account_activity_limit_exceeded              = 96,
    phone_activity_limit_exceeded                = 97,
    refund_to_wallet                             = 98,
    email_send_failure                           = 99,
    not_settled                                  = 100,
    need_captcha                                 = 101,
    gslt_denied                                  = 102,
    gs_owner_denied                              = 103,
    invalid_item_type                            = 104,
    ip_banned                                    = 105,
    gslt_expired                                 = 106,
    insufficient_funds                           = 107,
    too_many_pending                             = 108,
    no_site_licenses_found                       = 109,
    wg_network_send_exceeded                     = 110,
    account_not_friends                          = 111,
    limited_user_account                         = 112,
    cant_remove_item                             = 113,
    account_deleted                              = 114,
    existing_user_cancelled_license              = 115,
    community_cooldown                           = 116,
    no_launcher_specified                        = 117,
    must_agree_to_ssa                            = 118,
    launcher_migrated                            = 119,
    steam_realm_mismatch                         = 120,
    invalid_signature                            = 121,
    parse_failure                                = 122,
    no_verified_phone                            = 123,
    insufficient_battery                         = 124,
    charger_required                             = 125,
    cached_credential_invalid                    = 126,
    phone_number_is_voip                         = 127,
    not_supported                                = 128,
    family_size_limit_exceeded                   = 129,
    offline_app_cache_invalid                    = 130,
    try_later                                    = 131
  };

  std::string
  to_string (steam_result);

  std::string
  describe (steam_result);

  bool
  transient (steam_result) noexcept;

  class steam_exception : public std::runtime_error
  {
  public:
    steam_exception (steam_result, const std::string& context);

    steam_result
    result () const noexcept
    {
      return result_;
    }

    bool
    transient () const noexcept
    {
      return launcher::transient (result_);
    }

  private:
    steam_result result_;
  };

  void
  check_result (steam_result r, const std::string& context);
}
