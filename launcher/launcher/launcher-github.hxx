// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <launcher/github/github-api.hxx>
#include <launcher/manifest/manifest.hxx>

#include <boost/asio.hpp>

#include <string>
#include <optional>
#include <vector>
#include <regex>
#include <functional>

namespace launcher
{
  namespace asio = boost::asio;

  // GitHub releases of a repository and the manifests that they publish.
  //
  class github_coordinator
  {
  public:
    using api_type = github_api;
    using release_type = github_release;
    using asset_type = github_asset;
    using repository_type = github_repository;

    explicit
    github_coordinator (asio::io_context& ioc);

    // Authenticate the requests with the token (see set_token()).
    //
    github_coordinator (asio::io_context& ioc, std::string token);

    github_coordinator (const github_coordinator&) = delete;
    github_coordinator& operator= (const github_coordinator&) = delete;

    // Authenticate the requests with the token, which GitHub requires for
    // private repositories and which raises the rate limit.
    //
    void
    set_token (std::string token);

    // Send the requests through the proxy.
    //
    void
    set_proxy (std::string proxy_url);

    // Called while we wait for the rate limit to reset, with the message to
    // show and the number of seconds left.
    //
    using progress_callback_type =
      std::function<void (const std::string& message, std::uint64_t seconds_remaining)>;

    void
    set_progress_callback (progress_callback_type callback);

    // Return the latest release of the repository. If include_prerelease is
    // true, then this can also be a prerelease.
    //
    asio::awaitable<release_type>
    fetch_latest_release (std::string owner,
                          std::string repo,
                          bool include_prerelease = false);

    asio::awaitable<release_type>
    fetch_release_by_tag (std::string owner,
                          std::string repo,
                          std::string tag);

    // Download and parse the release's update.json asset, resolving the
    // URLs of its archives (see resolve_manifest_urls()). Throw
    // std::runtime_error if the release has no such asset and the manifest
    // parsing exceptions if it is invalid.
    //
    asio::awaitable<manifest>
    fetch_manifest (release_type release,
                    manifest_format kind = manifest_format::update);

    // As above but for the first asset whose name contains a match of the
    // regular expression.
    //
    asio::awaitable<manifest>
    fetch_manifest_by_pattern (release_type release,
                               std::string pattern,
                               manifest_format kind = manifest_format::update);

    // Return the asset with the specified name, if any.
    //
    std::optional<asset_type>
    find_asset (const release_type& release,
                const std::string& name) const;

    // Return the first asset whose name contains a match of the regular
    // expression, if any.
    //
    std::optional<asset_type>
    find_asset_regex (const release_type& release,
                      const std::string& pattern) const;

    // Set the URL of each manifest archive without one to the download URL
    // of its release asset, which we look up by the archive name and then by
    // the name as a regular expression. Archives without an asset keep the
    // empty URL.
    //
    void
    resolve_manifest_urls (manifest& m,
                           const release_type& release) const;

    asio::awaitable<repository_type>
    fetch_repository (std::string owner,
                      std::string repo);

    api_type&
    api () noexcept;

    const api_type&
    api () const noexcept;

  private:
    asio::awaitable<manifest>
    download_and_parse_manifest (std::string url,
                                 manifest_format kind);

    asio::io_context& ioc_;
    api_type api_;
  };

  // Return the assets whose names contain a match of the regular expression.
  //
  std::vector<github_asset>
  find_assets_regex (const github_release& release,
                     const std::string& pattern);

  // Return the asset's download URL.
  //
  std::string
  get_asset_download_url (const github_asset& asset);
}
