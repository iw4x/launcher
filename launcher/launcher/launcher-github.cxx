#include <launcher/launcher-github.hxx>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <boost/json.hpp>

#include <launcher/github/github-api.hxx>
#include <launcher/launcher-http.hxx>
#include <launcher/manifest/manifest.hxx>

using namespace std;

namespace launcher
{
  github_coordinator::
  github_coordinator (asio::io_context& c)
    : ioc_ (c),
      api_ (make_unique<api_type> (c))
  {
  }

  github_coordinator::
  github_coordinator (asio::io_context& c, string t)
    : ioc_ (c),
      api_ (make_unique<api_type> (c, move (t)))
  {
  }

  void github_coordinator::
  set_token (string t)
  {
    api_->set_token (move (t));
  }

  void github_coordinator::
  set_progress_callback (progress_callback_type cb)
  {
    api_->set_progress_callback (move (cb));
  }

  asio::awaitable<github_coordinator::release_type> github_coordinator::
  fetch_latest_release (const string& own,
                        const string& rep,
                        bool pre)
  {
    // GitHub's "latest" endpoint strictly returns the most recent stable
    // release. If we are willing to accept a pre-release (e.g., for staging
    // or nightly builds), we can't use that shortcut. We have to list them
    // and pick the top one.
    //
    if (pre)
    {
      // Fetch a small batch; the first one is the newest.
      //
      vector<release_type> rs (
        co_await api_->get_releases (own, rep, 10));

      if (rs.empty ())
        throw runtime_error ("no releases found for " + own + "/" + rep);

      co_return rs[0];
    }
    else
      co_return co_await api_->get_latest_release (own, rep);
  }

  asio::awaitable<github_coordinator::release_type> github_coordinator::
  fetch_release_by_tag (const string& own,
                        const string& rep,
                        const string& t)
  {
    co_return co_await api_->get_release_by_tag (own, rep, t);
  }

  // Manifest handling.
  //

  asio::awaitable<manifest> github_coordinator::
  fetch_manifest (const release_type& r, manifest_format fmt)
  {
    // In the standard layout, the manifest is always named 'update.json'.
    //
    const string n ("update.json");
    optional<asset_type> a (find_asset (r, n));

    if (!a)
      throw runtime_error ("manifest asset '" + n + "' not found in " +
                           r.tag_name);

    // Download, parse, and then stitch the URLs.
    //
    manifest m (
      co_await download_and_parse_manifest (a->browser_download_url, fmt));

    m.link_files ();
    resolve_manifest_urls (m, r);

    co_return m;
  }

  asio::awaitable<manifest> github_coordinator::
  fetch_manifest_by_pattern (const release_type& r,
                             const string& pat,
                             manifest_format fmt)
  {
    // If the manifest naming scheme varies (e.g., includes the version
    // number), we have to hunt for it.
    //
    optional<asset_type> a (find_asset_regex (r, pat));

    if (!a)
      throw runtime_error ("no manifest asset matching '" + pat +
                           "' found in " + r.tag_name);

    manifest m (
      co_await download_and_parse_manifest (a->browser_download_url, fmt));

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
    // The manifest knows the logical file names (e.g., 'release.tar.gz'), but
    // the GitHub release object holds the actual download URLs (which might
    // be signed AWS links). We need to bridge this gap so the rest of the
    // system has valid download links.
    //

    // Resolve Archive URLs.
    //
    for (auto& ar : m.archives)
    {
      // If the manifest already specifies an external URL (e.g. a mirror), we
      // respect it.
      //
      if (!ar.url.empty ())
        continue;

      // Try exact match first, falling back to regex if the logic requires
      // loose matching.
      //
      optional<asset_type> a (find_asset (r, ar.name));

      if (!a)
        a = find_asset_regex (r, ar.name);

      if (!a)
        throw runtime_error ("asset not found for archive: " + ar.name);

      ar.url = a->browser_download_url;
    }

    // We don't resolve standalone file URLs here. The assumption is that if
    // the launcher needs them, it will match the asset name at runtime.
    //
  }

  asio::awaitable<github_coordinator::repository_type> github_coordinator::
  fetch_repository (const string& own, const string& rep)
  {
    co_return co_await api_->get_repository (own, rep);
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
  download_and_parse_manifest (const string& u, manifest_format fmt)
  {
    http_coordinator h (ioc_);
    string s (co_await h.get (u));

    if (s.empty ())
      throw runtime_error ("manifest is empty");

    manifest m (s, fmt);
    co_return m;
  }

  // Standalone helpers.
  //

  vector<github_asset>
  find_assets_regex (const github_release& r, const string& pat)
  {
    regex re (pat);
    vector<github_asset> rs;

    for (const auto& a : r.assets)
    {
      if (regex_search (a.name, re))
        rs.push_back (a);
    }

    return rs;
  }

  string
  get_asset_download_url (const github_asset& a)
  {
    return a.browser_download_url;
  }
}
