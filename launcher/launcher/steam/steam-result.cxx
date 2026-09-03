#include <launcher/steam/steam-result.hxx>

using namespace std;

namespace launcher
{
  string
  to_string (steam_result r)
  {
    switch (r)
    {
      case steam_result::invalid:                         return "Invalid";
      case steam_result::ok:                              return "OK";
      case steam_result::fail:                            return "Fail";
      case steam_result::no_connection:                   return "NoConnection";
      case steam_result::invalid_password:                return "InvalidPassword";
      case steam_result::logged_in_elsewhere:             return "LoggedInElsewhere";
      case steam_result::invalid_protocol_ver:            return "InvalidProtocolVer";
      case steam_result::invalid_param:                   return "InvalidParam";
      case steam_result::file_not_found:                  return "FileNotFound";
      case steam_result::busy:                            return "Busy";
      case steam_result::invalid_state:                   return "InvalidState";
      case steam_result::invalid_name:                    return "InvalidName";
      case steam_result::invalid_email:                   return "InvalidEmail";
      case steam_result::duplicate_name:                  return "DuplicateName";
      case steam_result::access_denied:                   return "AccessDenied";
      case steam_result::timeout:                         return "Timeout";
      case steam_result::banned:                          return "Banned";
      case steam_result::account_not_found:               return "AccountNotFound";
      case steam_result::invalid_steam_id:                return "InvalidSteamID";
      case steam_result::service_unavailable:             return "ServiceUnavailable";
      case steam_result::not_logged_on:                   return "NotLoggedOn";
      case steam_result::pending:                         return "Pending";
      case steam_result::encryption_failure:              return "EncryptionFailure";
      case steam_result::insufficient_privilege:          return "InsufficientPrivilege";
      case steam_result::limit_exceeded:                  return "LimitExceeded";
      case steam_result::revoked:                         return "Revoked";
      case steam_result::expired:                         return "Expired";
      case steam_result::already_redeemed:                return "AlreadyRedeemed";
      case steam_result::duplicate_request:               return "DuplicateRequest";
      case steam_result::already_owned:                   return "AlreadyOwned";
      case steam_result::ip_not_found:                    return "IPNotFound";
      case steam_result::persist_failed:                  return "PersistFailed";
      case steam_result::locking_failed:                  return "LockingFailed";
      case steam_result::logon_session_replaced:          return "LogonSessionReplaced";
      case steam_result::connect_failed:                  return "ConnectFailed";
      case steam_result::handshake_failed:                return "HandshakeFailed";
      case steam_result::io_failure:                      return "IOFailure";
      case steam_result::remote_disconnect:               return "RemoteDisconnect";
      case steam_result::shopping_cart_not_found:         return "ShoppingCartNotFound";
      case steam_result::blocked:                         return "Blocked";
      case steam_result::ignored:                         return "Ignored";
      case steam_result::no_match:                        return "NoMatch";
      case steam_result::account_disabled:                return "AccountDisabled";
      case steam_result::service_read_only:               return "ServiceReadOnly";
      case steam_result::account_not_featured:            return "AccountNotFeatured";
      case steam_result::administrator_ok:                return "AdministratorOK";
      case steam_result::content_version:                 return "ContentVersion";
      case steam_result::try_another_cm:                  return "TryAnotherCM";
      case steam_result::password_required_to_kick_session:return "PasswordRequiredToKickSession";
      case steam_result::already_logged_in_elsewhere:     return "AlreadyLoggedInElsewhere";
      case steam_result::suspended:                       return "Suspended";
      case steam_result::cancelled:                       return "Cancelled";
      case steam_result::data_corruption:                 return "DataCorruption";
      case steam_result::disk_full:                       return "DiskFull";
      case steam_result::remote_call_failed:              return "RemoteCallFailed";
      case steam_result::password_unset:                  return "PasswordUnset";
      case steam_result::external_account_unlinked:       return "ExternalAccountUnlinked";
      case steam_result::psn_ticket_invalid:              return "PSNTicketInvalid";
      case steam_result::external_account_already_linked: return "ExternalAccountAlreadyLinked";
      case steam_result::remote_file_conflict:            return "RemoteFileConflict";
      case steam_result::illegal_password:                return "IllegalPassword";
      case steam_result::same_as_previous_value:          return "SameAsPreviousValue";
      case steam_result::account_logon_denied:            return "AccountLogonDenied";
      case steam_result::cannot_use_old_password:         return "CannotUseOldPassword";
      case steam_result::invalid_login_auth_code:         return "InvalidLoginAuthCode";
      case steam_result::account_logon_denied_no_mail:    return "AccountLogonDeniedNoMail";
      case steam_result::hardware_not_capable_of_ipt:     return "HardwareNotCapableOfIPT";
      case steam_result::ipt_init_error:                  return "IPTInitError";
      case steam_result::parental_control_restricted:     return "ParentalControlRestricted";
      case steam_result::facebook_query_error:            return "FacebookQueryError";
      case steam_result::expired_login_auth_code:         return "ExpiredLoginAuthCode";
      case steam_result::ip_login_restriction_failed:     return "IPLoginRestrictionFailed";
      case steam_result::account_locked_down:             return "AccountLockedDown";
      case steam_result::account_logon_denied_verified_email_required:return "AccountLogonDeniedVerifiedEmailRequired";
      case steam_result::no_matching_url:                 return "NoMatchingURL";
      case steam_result::bad_response:                    return "BadResponse";
      case steam_result::require_password_re_entry:       return "RequirePasswordReEntry";
      case steam_result::value_out_of_range:              return "ValueOutOfRange";
      case steam_result::unexpected_error:                return "UnexpectedError";
      case steam_result::disabled:                        return "Disabled";
      case steam_result::invalid_ceg_submission:          return "InvalidCEGSubmission";
      case steam_result::restricted_device:               return "RestrictedDevice";
      case steam_result::region_locked:                   return "RegionLocked";
      case steam_result::rate_limit_exceeded:             return "RateLimitExceeded";
      case steam_result::account_login_denied_need_two_factor:return "AccountLoginDeniedNeedTwoFactor";
      case steam_result::item_deleted:                    return "ItemDeleted";
      case steam_result::account_login_denied_throttle:   return "AccountLoginDeniedThrottle";
      case steam_result::two_factor_code_mismatch:        return "TwoFactorCodeMismatch";
      case steam_result::two_factor_activation_code_mismatch:return "TwoFactorActivationCodeMismatch";
      case steam_result::account_associated_to_multiple_partners:return "AccountAssociatedToMultiplePartners";
      case steam_result::not_modified:                    return "NotModified";
      case steam_result::no_mobile_device:                return "NoMobileDevice";
      case steam_result::time_not_synced:                 return "TimeNotSynced";
      case steam_result::sms_code_failed:                 return "SMSCodeFailed";
      case steam_result::account_limit_exceeded:          return "AccountLimitExceeded";
      case steam_result::account_activity_limit_exceeded: return "AccountActivityLimitExceeded";
      case steam_result::phone_activity_limit_exceeded:   return "PhoneActivityLimitExceeded";
      case steam_result::refund_to_wallet:                return "RefundToWallet";
      case steam_result::email_send_failure:              return "EmailSendFailure";
      case steam_result::not_settled:                     return "NotSettled";
      case steam_result::need_captcha:                    return "NeedCaptcha";
      case steam_result::gslt_denied:                     return "GSLTDenied";
      case steam_result::gs_owner_denied:                 return "GSOwnerDenied";
      case steam_result::invalid_item_type:               return "InvalidItemType";
      case steam_result::ip_banned:                       return "IPBanned";
      case steam_result::gslt_expired:                    return "GSLTExpired";
      case steam_result::insufficient_funds:              return "InsufficientFunds";
      case steam_result::too_many_pending:                return "TooManyPending";
      case steam_result::no_site_licenses_found:          return "NoSiteLicensesFound";
      case steam_result::wg_network_send_exceeded:        return "WGNetworkSendExceeded";
      case steam_result::account_not_friends:             return "AccountNotFriends";
      case steam_result::limited_user_account:            return "LimitedUserAccount";
      case steam_result::cant_remove_item:                return "CantRemoveItem";
      case steam_result::account_deleted:                 return "AccountDeleted";
      case steam_result::existing_user_cancelled_license: return "ExistingUserCancelledLicense";
      case steam_result::community_cooldown:              return "CommunityCooldown";
      case steam_result::no_launcher_specified:           return "NoLauncherSpecified";
      case steam_result::must_agree_to_ssa:               return "MustAgreeToSSA";
      case steam_result::launcher_migrated:               return "LauncherMigrated";
      case steam_result::steam_realm_mismatch:            return "SteamRealmMismatch";
      case steam_result::invalid_signature:               return "InvalidSignature";
      case steam_result::parse_failure:                   return "ParseFailure";
      case steam_result::no_verified_phone:               return "NoVerifiedPhone";
      case steam_result::insufficient_battery:            return "InsufficientBattery";
      case steam_result::charger_required:                return "ChargerRequired";
      case steam_result::cached_credential_invalid:       return "CachedCredentialInvalid";
      case steam_result::phone_number_is_voip:            return "PhoneNumberIsVOIP";
      case steam_result::not_supported:                   return "NotSupported";
      case steam_result::family_size_limit_exceeded:      return "FamilySizeLimitExceeded";
      case steam_result::offline_app_cache_invalid:       return "OfflineAppCacheInvalid";
      case steam_result::try_later:                       return "TryLater";
    }

    return "EResult(" + std::to_string (static_cast<int32_t> (r)) + ")";
  }

  string
  describe (steam_result r)
  {
    switch (r)
    {
    case steam_result::ok:
      return "success";

    case steam_result::invalid_password:
      return "the account name or password is incorrect";

    case steam_result::account_login_denied_need_two_factor:
      return "the account is protected by the Steam Mobile Authenticator and "
             "requires a device code";

    case steam_result::account_logon_denied:
      return "the account is protected by Steam Guard and requires the code "
             "that was emailed to it";

    case steam_result::account_logon_denied_no_mail:
      return "the account requires Steam Guard email verification, but no "
             "email address is associated with it";

    case steam_result::invalid_login_auth_code:
      return "the Steam Guard code was rejected";

    case steam_result::expired_login_auth_code:
      return "the Steam Guard code has expired; request a new one";

    case steam_result::two_factor_code_mismatch:
      return "the two-factor code was rejected; check that the device clock "
             "is synchronized";

    case steam_result::rate_limit_exceeded:
    case steam_result::account_login_denied_throttle:
      return "too many login attempts from this address; wait before trying "
             "again";

    case steam_result::access_denied:
      return "the account does not have access to this content";

    case steam_result::invalid_state:
      return "the request was rejected because the session is in the wrong "
             "state";

    case steam_result::not_logged_on:
      return "the session is not logged on";

    case steam_result::logon_session_replaced:
    case steam_result::logged_in_elsewhere:
    case steam_result::already_logged_in_elsewhere:
      return "the session was displaced by another logon for the same "
             "account";

    case steam_result::file_not_found:
      return "the requested content does not exist on the server";

    case steam_result::service_unavailable:
    case steam_result::busy:
      return "the Steam service is temporarily unavailable";

    case steam_result::try_another_cm:
      return "this connection manager declined the session; another will be "
             "tried";

    case steam_result::timeout:
      return "the request timed out";

    case steam_result::no_connection:
      return "there is no connection to Steam";

    case steam_result::banned:
    case steam_result::account_disabled:
    case steam_result::account_locked_down:
      return "the account is not permitted to perform this operation";

    case steam_result::region_locked:
      return "the content is not available in this region";

    case steam_result::disk_full:
      return "there is not enough free disk space";

    case steam_result::data_corruption:
      return "the server reported that the content is corrupt";

    default:
      break;
    }

    return to_string (r);
  }

  bool
  transient (steam_result r) noexcept
  {
    switch (r)
    {
    case steam_result::busy:
    case steam_result::no_connection:
    case steam_result::service_unavailable:
    case steam_result::timeout:
    case steam_result::try_another_cm:
    case steam_result::io_failure:
    case steam_result::remote_disconnect:
    case steam_result::remote_call_failed:
    case steam_result::connect_failed:
    case steam_result::handshake_failed:
    case steam_result::bad_response:
    case steam_result::try_later:
      return true;

    default:
      return false;
    }
  }

  steam_exception::
  steam_exception (steam_result r, const string& c)
    : runtime_error ("failed to " + c + ": " + describe (r) +
                     " [" + to_string (r) + "]"),
      result_ (r)
  {
  }

  void
  check_result (steam_result r, const string& c)
  {
    if (r != steam_result::ok)
      throw steam_exception (r, c);
  }
}
