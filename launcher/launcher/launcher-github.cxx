#include <launcher/launcher-github.hxx>

#include <boost/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <regex>

#include <launcher/github/github-api.hxx>
#include <launcher/launcher-http.hxx>
#include <launcher/manifest/manifest.hxx>

using namespace std;

namespace launcher
{
  github_coordinator::
  github_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      api_ (make_unique<api_type> (ioc))
  {
  }

  github_coordinator::
  github_coordinator (asio::io_context& ioc, string token)
    : ioc_ (ioc),
      api_ (make_unique<api_type> (ioc, move (token)))
  {
  }

  void github_coordinator::
  set_token (string t)
  {
    api_->set_token (move (t));
  }

  asio::awaitable<github_coordinator::release_type> github_coordinator::
  fetch_latest_release (const string& owner,
                        const string& repo,
                        bool include_prerelease)
  {
    // If the caller wants pre-releases, we can't use the simple "latest"
    // endpoint because GitHub's API defines "latest" strictly as the most
    // recent stable release. So we have to fetch the list and pick the top
    // one.
    //
    if (include_prerelease)
    {
      // Fetch a small batch; the first one should be the newest.
      //
      vector<release_type> rs (
        co_await api_->get_releases (owner, repo, 10));

      if (rs.empty ())
        throw runtime_error ("no releases found for " + owner + "/" + repo);

      co_return rs[0];
    }
    else
    {
      co_return co_await api_->get_latest_release (owner, repo);
    }
  }

  asio::awaitable<github_coordinator::release_type> github_coordinator::
  fetch_release_by_tag (const string& owner,
                        const string& repo,
                        const string& tag)
  {
    co_return co_await api_->get_release_by_tag (owner, repo, tag);
  }

  // Manifest handling.
  //

  asio::awaitable<manifest> github_coordinator::
  fetch_manifest (const release_type& r, manifest_format k)
  {
    // Usually, the manifest is just named 'update.json'.
    //
    const string n ("update.json");

    optional<asset_type> a (find_asset (r, n));

    if (!a)
      throw runtime_error ("manifest asset '" + n + "' not found in " +
                           r.tag_name);

    // Download and parse.
    //
    manifest m (
      co_await download_and_parse_manifest (a->browser_download_url, k));

    // Post-process: link the files structure and resolve the actual download
    // URLs from the release assets.
    //
    m.link_files ();
    resolve_manifest_urls (m, r);

    co_return m;
  }

  asio::awaitable<manifest> github_coordinator::
  fetch_manifest_by_pattern (const release_type& r,
                             const string& pat,
                             manifest_format k)
  {
    // Sometimes the manifest name varies (e.g., version numbers in the
    // filename), so we hunt for it via regex.
    //
    optional<asset_type> a (find_asset_regex (r, pat));

    if (!a)
      throw runtime_error ("no manifest asset matching '" + pat +
                           "' found in " + r.tag_name);

    manifest m (
      co_await download_and_parse_manifest (a->browser_download_url, k));

    m.link_files ();
    resolve_manifest_urls (m, r);

    co_return m;
  }

  // Asset lookup.
  //

  optional<github_coordinator::asset_type> github_coordinator::
  find_asset (const release_type& r, const string& n) const
  {
    for (const auto& a : r.assets)
    {
      if (a.name == n)
        return a;
    }

    return nullopt;
  }

  optional<github_coordinator::asset_type> github_coordinator::
  find_asset_regex (const release_type& r, const string& pat) const
  {
    regex re (pat);

    for (const auto& a : r.assets)
    {
      if (regex_search (a.name, re))
        return a;
    }

    return nullopt;
  }

  void github_coordinator::
  resolve_manifest_urls (manifest& m, const release_type& r) const
  {
    // The manifest knows the logical file names (e.g., 'release.zip'), but the
    // release object holds the actual download URLs (which might be signed AWS
    // links). We need to bridge this gap.
    //

    // Firt, resolve Archive URLs.
    //
    for (auto& ar : m.archives)
    {
      // If the manifest already has a URL (e.g. external mirror), skip it.
      //
      if (!ar.url.empty ())
        continue;

      // Try exact match first, then regex.
      //
      optional<asset_type> a (find_asset (r, ar.name));

      if (!a)
        a = find_asset_regex (r, ar.name);

      if (!a)
        throw runtime_error ("asset not found for archive: " + ar.name);

      ar.url = a->browser_download_url;
    }

    // Next, resolve standalone files.
    //
    // Note: Individual files in the manifest don't store URLs directly in this
    // format version. The launcher is expected to match the 'asset_name'
    // against the release assets at runtime if it needs to download them
    // individually.
    //
  }

  asio::awaitable<github_coordinator::repository_type> github_coordinator::
  fetch_repository (const string& owner, const string& repo)
  {
    co_return co_await api_->get_repository (owner, repo);
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
  download_and_parse_manifest (const string& url, manifest_format k)
  {
    http_coordinator http (ioc_);
    string json (co_await http.get (url));

    if (json.empty ())
      throw runtime_error ("manifest JSON is empty");

    manifest m (json, k);
    co_return m;
  }

  // Standalone helpers.
  //

  vector<github_asset>
  find_assets_regex (const github_release& r, const string& pat)
  {
    regex re (pat);
    vector<github_asset> res;

    for (const auto& a : r.assets)
    {
      if (regex_search (a.name, re))
        res.push_back (a);
    }

    return res;
  }

  string
  get_asset_download_url (const github_asset& a)
  {
    return a.browser_download_url;
  }
}
