#pragma once

#include <hello/github/github-api.hxx>
#include <hello/manifest/manifest.hxx>

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <regex>

namespace hello
{
  namespace asio = boost::asio;

  class github_coordinator
  {
  public:
    using api_type = github_api<>;
    using release_type = github_release;
    using asset_type = github_asset;
    using repository_type = github_repository;

    // Constructors.
    //
    explicit
    github_coordinator (asio::io_context& ioc);

    github_coordinator (asio::io_context& ioc, std::string token);

    github_coordinator (const github_coordinator&) = delete;
    github_coordinator& operator= (const github_coordinator&) = delete;

    // Set authentication token.
    //
    // Required for higher rate limits and private repositories.
    //
    void
    set_token (std::string token);

    // Fetch latest release.
    //
    // If include_prerelease is true, returns the most recent release
    // (including prereleases). Otherwise, returns the latest stable release.
    //
    asio::awaitable<release_type>
    fetch_latest_release (const std::string& owner,
                          const std::string& repo,
                          bool include_prerelease = false);

    // Fetch release by tag.
    //
    asio::awaitable<release_type>
    fetch_release_by_tag (const std::string& owner,
                          const std::string& repo,
                          const std::string& tag);

    // Fetch manifest from release.
    //
    // Looks for an asset named "update.json" or matching the manifest
    // pattern, downloads it, parses the JSON, and returns the manifest.
    //
    // Throws if the manifest asset is not found or parsing fails.
    //
    asio::awaitable<manifest>
    fetch_manifest (const release_type& release,
                    manifest_format kind = manifest_format::update);

    // Fetch manifest by pattern.
    //
    // Searches for an asset matching the given regex pattern.
    //
    asio::awaitable<manifest>
    fetch_manifest_by_pattern (const release_type& release,
                               const std::string& pattern,
                               manifest_format kind = manifest_format::update);

    // Find asset by name.
    //
    // Returns the first asset whose name matches the given string exactly.
    //
    std::optional<asset_type>
    find_asset (const release_type& release,
                const std::string& name) const;

    // Find asset by regex pattern.
    //
    // Returns the first asset whose name matches the given regex.
    //
    std::optional<asset_type>
    find_asset_regex (const release_type& release,
                      const std::string& pattern) const;

    // Resolve asset URLs for manifest.
    //
    // Links manifest archives and files to their corresponding GitHub release
    // assets, populating the URL fields.
    //
    // Throws if required assets are missing.
    //
    void
    resolve_manifest_urls (manifest& m,
                           const release_type& release) const;

    // Fetch repository information.
    //
    asio::awaitable<repository_type>
    fetch_repository (const std::string& owner,
                      const std::string& repo);

    // Access underlying API client.
    //
    api_type&
    api () noexcept;

    const api_type&
    api () const noexcept;

  private:
    // Download and parse manifest JSON from URL.
    //
    asio::awaitable<manifest>
    download_and_parse_manifest (const std::string& url,
                                 manifest_format kind);

    asio::io_context& ioc_;
    std::unique_ptr<api_type> api_;
  };

  // Find all assets matching a pattern.
  //
  // Returns a vector of all assets whose names match the regex.
  //
  std::vector<github_asset>
  find_assets_regex (const github_release& release,
                     const std::string& pattern);

  // Get download URL for an asset.
  //
  // Returns the browser_download_url, which is the direct download link.
  //
  std::string
  get_asset_download_url (const github_asset& asset);
}
