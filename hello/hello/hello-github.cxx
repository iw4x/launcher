#include <hello/hello-github.hxx>
#include <hello/hello-http.hxx>

#include <hello/github/github-api.hxx>
#include <hello/manifest/manifest.hxx>

#include <boost/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace hello
{
  namespace fs = std::filesystem;
  namespace json = boost::json;

  github_coordinator::
  github_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      api_ (std::make_unique<api_type> (ioc))
  {
  }

  github_coordinator::
  github_coordinator (asio::io_context& ioc, std::string token)
    : ioc_ (ioc),
      api_ (std::make_unique<api_type> (ioc, std::move (token)))
  {
  }

  void github_coordinator::
  set_token (std::string token)
  {
    api_->set_token (std::move (token));
  }

  asio::awaitable<github_coordinator::release_type> github_coordinator::
  fetch_latest_release (const std::string& owner,
                        const std::string& repo,
                        bool include_prerelease)
  {
    if (include_prerelease)
    {
      // Get all releases and return the first one.
      //
      // The GitHub API returns releases in reverse chronological order, so
      // the first release is the most recent (including prereleases).
      //
      std::vector<release_type> releases (
          co_await api_->get_releases (owner, repo, 10));

      if (releases.empty ())
      {
        throw std::runtime_error (
            "no releases found for " + owner + "/" + repo);
      }

      co_return releases[0];
    }
    else
    {
      // Use the dedicated "latest release" endpoint.
      //
      release_type release (
          co_await api_->get_latest_release (owner, repo));

      co_return release;
    }
  }

  asio::awaitable<github_coordinator::release_type> github_coordinator::
  fetch_release_by_tag (const std::string& owner,
                        const std::string& repo,
                        const std::string& tag)
  {
    release_type release (
        co_await api_->get_release_by_tag (owner, repo, tag));

    co_return release;
  }

  asio::awaitable<manifest> github_coordinator::
  fetch_manifest (const release_type& release,
                  manifest_format kind)
  {
    const std::string manifest_name ("update.json");

    std::optional<asset_type> asset (find_asset (release, manifest_name));

    if (!asset)
      throw std::runtime_error ("manifest asset '" + manifest_name +
                                "' not found in release " + release.tag_name);

    manifest m (
      co_await download_and_parse_manifest (asset->browser_download_url, kind));

    m.link_files ();

    resolve_manifest_urls (m, release);

    co_return m;
  }

  asio::awaitable<manifest> github_coordinator::
  fetch_manifest_by_pattern (const release_type& release,
                             const std::string& pattern,
                             manifest_format kind)
  {
    std::optional<asset_type> asset (find_asset_regex (release, pattern));

    if (!asset)
      throw std::runtime_error ("no manifest asset matching pattern '" +
                                pattern + "' found in release " +
                                release.tag_name);

    manifest m (
      co_await download_and_parse_manifest (asset->browser_download_url, kind));

    m.link_files ();
    resolve_manifest_urls (m, release);

    co_return m;
  }

  std::optional<github_coordinator::asset_type> github_coordinator::
  find_asset (const release_type& release,
              const std::string& name) const
  {
    for (const auto& asset : release.assets)
    {
      if (asset.name == name)
        return asset;
    }

    return std::nullopt;
  }

  std::optional<github_coordinator::asset_type> github_coordinator::
  find_asset_regex (const release_type& release,
                    const std::string& pattern) const
  {
    std::regex re (pattern);

    for (const auto& asset : release.assets)
    {
      if (std::regex_search (asset.name, re))
        return asset;
    }

    return std::nullopt;
  }

  void github_coordinator::
  resolve_manifest_urls (manifest& m,
                         const release_type& release) const
  {
    // Resolve archive URLs.
    //
    // For each archive in the manifest, find the corresponding release asset
    // and populate the URL field.
    //
    for (auto& archive : m.archives)
    {
      if (!archive.url.empty ())
        continue; // already has url

      std::optional<asset_type> asset (find_asset (release, archive.name));

      if (!asset)
        asset = find_asset_regex (release, archive.name);

      if (!asset)
        throw std::runtime_error ("asset not found for archive: " +
                                  archive.name);

      archive.url = asset->browser_download_url;
    }

    // Resolve standalone file URLs.
    //
    // For files that are not in archives (i.e., they have asset_name but no
    // archive_name), find the asset and populate the URL if needed.
    //
    // Note: The manifest structure doesn't directly store URLs for individual
    // files. The launcher must use the asset_name to find the download URL
    // from the release.
    //
    // This is a design constraint: manifests describe files and their hashes,
    // but the launcher must resolve asset names to URLs using the release
    // metadata.
  }

  asio::awaitable<github_coordinator::repository_type> github_coordinator::
  fetch_repository (const std::string& owner,
                    const std::string& repo)
  {
    repository_type repository (co_await api_->get_repository (owner, repo));

    co_return repository;
  }

  github_coordinator::api_type& github_coordinator::
  api () noexcept
  {
    return *api_;
  }

  const github_coordinator::api_type& github_coordinator::
  api () const noexcept
  {
    return *api_;
  }

  asio::awaitable<manifest> github_coordinator::
  download_and_parse_manifest (const std::string& url,
                               manifest_format kind)
  {
    http_coordinator http (ioc_);
    std::string json_content (co_await http.get (url));

    if (json_content.empty ())
      throw std::runtime_error ("manifest JSON is empty");

    manifest m (json_content, kind);

    co_return m;
  }

  // Helper.
  //

  std::vector<github_asset>
  find_assets_regex (const github_release& release,
                     const std::string& pattern)
  {
    std::regex re (pattern);
    std::vector<github_asset> result;

    for (const auto& asset : release.assets)
    {
      if (std::regex_search (asset.name, re))
        result.push_back (asset);
    }

    return result;
  }

  std::string
  get_asset_download_url (const github_asset& asset)
  {
    return asset.browser_download_url;
  }
}
