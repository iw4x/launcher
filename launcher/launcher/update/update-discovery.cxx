#include <launcher/update/update-discovery.hxx>

#include <algorithm>
#include <cctype>
#include <stdexcept>

using namespace std;

namespace launcher
{
  update_discovery::
  update_discovery (asio::io_context& c)
    : ioc_ (c),
      api_ (make_unique<api_type> (c))
  {
  }

  update_discovery::
  update_discovery (asio::io_context& c, string t)
    : ioc_ (c),
      api_ (make_unique<api_type> (c, move (t)))
  {
  }

  void update_discovery::
  set_token (string t)
  {
    api_->set_token (move (t));
  }

  void update_discovery::
  set_progress_callback (progress_callback_type c)
  {
    api_->set_progress_callback (move (c));
  }

  void update_discovery::
  set_include_prerelease (bool v)
  {
    include_prerelease_ = v;
  }

  bool update_discovery::
  include_prerelease () const noexcept
  {
    return include_prerelease_;
  }

  asio::awaitable<update_info> update_discovery::
  check_for_update (const string& o,
                    const string& r,
                    const launcher_version& v)
  {
    update_info ui (co_await fetch_latest_release (o, r));

    // We might get an empty result if we filtered out all available releases
    // (e.g., they were all drafts).
    //
    if (ui.empty ())
      co_return update_info ();

    // Compare what we found with what we are running. Only bother returning
    // the info if it's actually an upgrade.
    //
    if (ui.version > v)
      co_return ui;

    co_return update_info ();
  }

  asio::awaitable<update_info> update_discovery::
  fetch_latest_release (const string& o, const string& r)
  {
    // Grab a batch of releases. We ask for 20 hoping that if we are ignoring
    // prereleases, we will find at least one stable release in this set.
    //
    vector<release_type> rs (
      co_await api_->get_releases (o, r, 20));

    if (rs.empty ())
      co_return update_info ();

    // Iterate through what we got and pick the winner. Note that the API
    // returns them sorted by creation date (newest first).
    //
    const release_type* b (nullptr);

    for (const auto& i : rs)
    {
      if (i.draft)
        continue;

      // If we are okay with prereleases, the first non-draft is the newest.
      // Otherwise, keep digging until we hit a stable one.
      //
      if (include_prerelease_ || !i.prerelease)
      {
        b = &i;
        break;
      }
    }

    if (b == nullptr)
      co_return update_info ();

    co_return release_to_update_info (*b);
  }

  asio::awaitable<update_info> update_discovery::
  fetch_release_by_tag (const string& o,
                        const string& r,
                        const string& t)
  {
    release_type rel (
      co_await api_->get_release_by_tag (o, r, t));

    if (rel.empty ())
      co_return update_info ();

    co_return release_to_update_info (rel);
  }

  update_discovery::api_type& update_discovery::
  api () noexcept
  {
    return *api_;
  }

  const update_discovery::api_type& update_discovery::
  api () const noexcept
  {
    return *api_;
  }

  update_info update_discovery::
  release_to_update_info (const release_type& r) const
  {
    update_info ui;

    // Before we bother parsing versions, let's make sure there is actually a
    // binary for us to download. If not, this release is useless to the
    // current platform.
    //
    auto a (find_platform_asset (r));
    if (!a)
      return ui;

    // While the release tag (e.g., "v1.2.3") is useful, the asset name
    // usually contains the full, canonical version (including snapshot IDs,
    // e.g., "1.2.3-a.1-20260201010251.fe4660334ed0"). Try to extract that
    // first.
    //
    auto v (parse_asset_version (a->name));
    if (!v)
    {
      // The asset naming scheme might have changed or be non-standard. Fall
      // back to the git tag.
      //
      v = parse_launcher_version (r.tag_name);
      if (!v)
        return ui;
    }

    ui.version = *v;
    ui.tag_name = r.tag_name;
    ui.release_url = r.html_url;
    ui.prerelease = r.prerelease;
    ui.body = r.body;
    ui.asset_url = a->browser_download_url;
    ui.asset_name = a->name;
    ui.asset_size = a->size;

    return ui;
  }

  optional<update_discovery::asset_type> update_discovery::
  find_platform_asset (const release_type& r) const
  {
    platform_type p (current_platform ());

    if (p == platform_type::unknown)
      return nullopt;

    // Hardcode the suffixes we expect the build system to produce for the
    // supported platforms.
    //
    string s;
    string e;

    switch (p)
    {
    case platform_type::windows_x64:
      s = "x86_64-windows";
      e = ".zip";
      break;
    case platform_type::linux_x64:
      s = "x86_64-linux-glibc";
      e = ".tar.xz";
      break;
    default:
      return nullopt;
    }

    // Scan the assets. We look for the standard pattern:
    // launcher-<version>-<platform>.<ext>
    //
    for (const auto& a : r.assets)
    {
      if (a.name.find (s) != string::npos &&
          a.name.find (e) != string::npos &&
          a.name.find ("launcher-") == 0)
      {
        return a;
      }
    }

    return nullopt;
  }

  optional<launcher_version> update_discovery::
  parse_asset_version (const string& n) const
  {
    // The plan is to strip the known prefix ("launcher-") and the known
    // platform suffix. Whatever is left in the middle should be the version.
    //
    const string p ("launcher-");
    if (n.find (p) != 0)
      return nullopt;

    const vector<string> sufs = {
      "-x86_64-windows",
      "-x86_64-linux-glibc"
    };

    size_t b (p.size ());
    size_t e (string::npos);

    // See if the name contains one of our known platform suffixes.
    //
    for (const auto& s : sufs)
    {
      size_t pos (n.find (s));
      if (pos != string::npos)
      {
        e = pos;
        break;
      }
    }

    // Sanity check: suffix must appears after the prefix.
    //
    if (e == string::npos || e <= b)
      return nullopt;

    string s (n.substr (b, e - b));
    return parse_launcher_version (s);
  }
}
