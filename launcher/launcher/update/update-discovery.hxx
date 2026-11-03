// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <boost/asio.hpp>

#include <functional>
#include <optional>
#include <string>

#include <launcher/update/update-types.hxx>

#include <launcher/github/github-api.hxx>

namespace launcher
{
  namespace asio = boost::asio;

  // Launcher releases on GitHub, as update_info for the current platform.
  // We only fetch the release metadata here (see update_installer for the
  // download).
  //
  class update_discovery
  {
  public:
    using api_type = github_api;
    using release_type = github_release;
    using asset_type = github_asset;

    // Called every second while we wait out the GitHub rate limit.
    //
    using progress_callback_type =
      std::function<void (const std::string& message,
                          std::uint64_t seconds_remaining)>;

    explicit
    update_discovery (asio::io_context& ioc);

    update_discovery (asio::io_context& ioc, std::string token);

    update_discovery (const update_discovery&) = delete;
    update_discovery& operator= (const update_discovery&) = delete;

    // Configuration.
    //

    void
    set_token (std::string token);

    void
    set_progress_callback (progress_callback_type callback);

    // If include is true, then also consider pre-releases. By default we
    // only consider final releases.
    //
    void
    set_include_prerelease (bool include);

    bool
    include_prerelease () const noexcept;

    // Discovery.
    //

    // Return the latest release in owner/repo (see fetch_latest_release())
    // if it is newer than current_version and empty info otherwise.
    //
    asio::awaitable<update_info>
    check_for_update (std::string owner,
                      std::string repo,
                      launcher_version current_version);

    // Return the latest release in owner/repo, skipping drafts and, unless
    // include_prerelease() is true, pre-releases. Return empty info if there
    // is none or it has no asset for the current platform.
    //
    // Note that we only look at the 20 most recent releases.
    //
    asio::awaitable<update_info>
    fetch_latest_release (std::string owner,
                          std::string repo);

    // Return the release with the tag in owner/repo. Return empty info if
    // there is none or it has no asset for the current platform.
    //
    asio::awaitable<update_info>
    fetch_release_by_tag (std::string owner,
                          std::string repo,
                          std::string tag);

    // Return the underlying GitHub API client.
    //
    api_type&
    api () noexcept;

    const api_type&
    api () const noexcept;

  private:
    // Return the release as update_info. Return empty info if it has no
    // asset for the current platform or we cannot determine its version.
    //
    update_info
    release_to_update_info (const release_type& release) const;

    // Return the release asset for the current platform, which is
    // launcher-<version>-<platform>.zip on Windows and .tar.xz on Linux.
    //
    std::optional<asset_type>
    find_platform_asset (const release_type& release) const;

    // Return the version in the asset name (see find_platform_asset()).
    //
    std::optional<launcher_version>
    parse_asset_version (const std::string& asset_name) const;

    api_type api_;
    bool include_prerelease_ = false;
  };
}
