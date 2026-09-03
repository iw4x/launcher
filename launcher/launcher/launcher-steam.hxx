#pragma once

#include <launcher/http/http-client.hxx>
#include <launcher/launcher-progress.hxx>
#include <launcher/steam/steam-auth.hxx>
#include <launcher/steam/steam-cm-client.hxx>
#include <launcher/steam/steam-content.hxx>
#include <launcher/steam/steam-credential-store.hxx>
#include <launcher/steam/steam-depot.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace launcher
{
  namespace fs = std::filesystem;

  struct steam_install_options
  {
    std::vector<steam_depot_target> targets;

    std::string account_name;

    bool use_qr = false;

    bool remember = true;

    bool force_login = false;

    bool force_download = false;

    std::uint32_t cell_id = 0;

    steam_depot_traits depot;
  };

  std::vector<steam_depot_target>
  steam_default_targets ();

  class steam_coordinator
  {
  public:
    steam_coordinator (asio::io_context&,
                       http_client_traits,
                       fs::path cache_directory);

    steam_coordinator (const steam_coordinator&) = delete;
    steam_coordinator& operator= (const steam_coordinator&) = delete;

    asio::awaitable<steam_depot_result>
    install (const steam_install_options&,
             const fs::path& root,
             progress_coordinator* progress);

    void
    forget (const std::string& account_name);

    credential_backend
    credential_backend_kind () const noexcept
    {
      return credentials_.backend ();
    }

  private:
    asio::awaitable<void>
    authenticate (steam_cm_client&, const steam_install_options&);

    asio::awaitable<steam_auth_tokens>
    interactive_login (steam_cm_client&, const steam_install_options&);

    std::string
    preferred_account (const steam_install_options&) const;

    void
    remember_account (const std::string&);

    std::string
    last_account () const;

    std::uint64_t
    installed_manifest (std::uint32_t depot_id) const;

    void
    record_installed_manifest (std::uint32_t depot_id,
                               std::uint64_t manifest_id);

  private:
    asio::io_context&      ioc_;
    http_client_traits     http_traits_;
    fs::path               cache_directory_;
    steam_credential_store credentials_;
  };
}
