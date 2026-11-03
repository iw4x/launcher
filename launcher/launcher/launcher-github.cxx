// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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
      api_ (c)
  {
  }

  github_coordinator::
  github_coordinator (asio::io_context& c, string t)
    : ioc_ (c),
      api_ (c, move (t))
  {
  }

  void github_coordinator::
  set_token (string t)
  {
    api_.set_token (move (t));
  }

  void github_coordinator::
  set_proxy (string u)
  {
    api_.set_proxy (move (u));
  }

  void github_coordinator::
  set_progress_callback (progress_callback_type cb)
  {
    api_.set_progress_callback (move (cb));
  }

  asio::awaitable<github_coordinator::release_type> github_coordinator::
  fetch_latest_release (const string& own,
                        const string& rep,
                        bool pre)
  {
    // GitHub's latest release is the latest stable one. To also consider the
    // prereleases we list the releases, which GitHub returns newest first.
    //
    if (pre)
    {
      vector<release_type> rs (
        co_await api_.get_releases (own, rep, 10));

      if (rs.empty ())
        throw runtime_error ("no releases found for " + own + "/" + rep);

      co_return rs[0];
    }
    else
      co_return co_await api_.get_latest_release (own, rep);
  }

  asio::awaitable<github_coordinator::release_type> github_coordinator::
  fetch_release_by_tag (const string& own,
                        const string& rep,
                        const string& t)
  {
    co_return co_await api_.get_release_by_tag (own, rep, t);
  }

  asio::awaitable<manifest> github_coordinator::
  fetch_manifest (const release_type& r, manifest_format fmt)
  {
    const string n ("update.json");
    optional<asset_type> a (find_asset (r, n));

    if (!a)
      throw runtime_error ("manifest asset '" + n + "' not found in " +
                           r.tag_name);

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
    for (auto& ar : m.archives)
    {
      // An archive with a URL is hosted elsewhere (e.g., on a mirror).
      //
      if (!ar.url.empty ())
        continue;

      optional<asset_type> a (find_asset (r, ar.name));

      if (!a)
        a = find_asset_regex (r, ar.name);

      if (!a)
        continue;

      ar.url = a->browser_download_url;
    }

    // Note that a file names its release asset (asset_name), which whoever
    // downloads the file looks up.
    //
  }

  asio::awaitable<github_coordinator::repository_type> github_coordinator::
  fetch_repository (const string& own, const string& rep)
  {
    co_return co_await api_.get_repository (own, rep);
  }

  github_coordinator::api_type& github_coordinator::
  api () noexcept
  {
    return api_;
  }

  const github_coordinator::api_type& github_coordinator::
  api () const noexcept
  {
    return api_;
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
