#pragma once

#include <boost/asio.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <launcher/update/update-types.hxx>

#include <launcher/github/github-api.hxx>

namespace launcher
{
  namespace asio = boost::asio;

  // Query GitHub for releases.
  //
  // We don't download the actual bits here, just figure out if there is
  // anything worth downloading. We handle the GitHub API specifics and try to
  // map their release model to our update_info.
  //
  class update_discovery
  {
  public:
    using api_type = github_api<>;
    using release_type = github_release;
    using asset_type = github_asset;

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

    void
    set_include_prerelease (bool include);

    bool
    include_prerelease () const noexcept;

    // Discovery.
    //

    // See if there is anything newer than current_version in owner/repo.
    //
    // Return the new version info if we find a candidate that is strictly
    // greater than current_version and has a valid asset for our platform.
    // Otherwise return empty info.
    //
    asio::awaitable<update_info>
    check_for_update (const std::string& owner,
                      const std::string& repo,
                      const launcher_version& current_version);

    // Just grab the latest one from owner/repo.
    //
    // Note that we don't compare versions here. Its mostly useful if we just
    // want to show the user what's currently out there.
    //
    asio::awaitable<update_info>
    fetch_latest_release (const std::string& owner,
                          const std::string& repo);

    // Fetch a specific release by tag.
    //
    asio::awaitable<update_info>
    fetch_release_by_tag (const std::string& owner,
                          const std::string& repo,
                          const std::string& tag);

    // Access underlying API client.
    //
    api_type&
    api () noexcept;

    const api_type&
    api () const noexcept;

  private:
    // Convert GitHub release to update_info.
    //
    update_info
    release_to_update_info (const release_type& release) const;

    // Find the appropriate asset for the current platform.
    //
    std::optional<asset_type>
    find_platform_asset (const release_type& release) const;

    // Extract version from release asset name.
    //
    // launcher-<version>-<platform>.<ext>
    //
    std::optional<launcher_version>
    parse_asset_version (const std::string& asset_name) const;

    asio::io_context& ioc_;
    std::unique_ptr<api_type> api_;
    bool include_prerelease_ = false;
  };
}
