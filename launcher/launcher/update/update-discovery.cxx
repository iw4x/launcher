#include <launcher/update/update-discovery.hxx>

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <launcher/launcher-log.hxx>

using namespace std;

namespace launcher
{
  update_discovery::
  update_discovery (asio::io_context& c)
    : ioc_ (c),
      api_ (make_unique<api_type> (c))
  {
    launcher::log::trace_l2 (categories::update{}, "initialized update_discovery (no token)");
  }

  update_discovery::
  update_discovery (asio::io_context& c, string t)
    : ioc_ (c),
      api_ (make_unique<api_type> (c, move (t)))
  {
    launcher::log::trace_l2 (categories::update{}, "initialized update_discovery (with token)");
  }

  void update_discovery::
  set_token (string t)
  {
    launcher::log::trace_l3 (categories::update{}, "updating github api token");
    api_->set_token (move (t));
  }

  void update_discovery::
  set_progress_callback (progress_callback_type c)
  {
    launcher::log::trace_l3 (categories::update{}, "setting progress callback");
    api_->set_progress_callback (move (c));
  }

  void update_discovery::
  set_include_prerelease (bool v)
  {
    launcher::log::trace_l3 (categories::update{}, "set_include_prerelease: {}", v);
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
    launcher::log::info (categories::update{}, "checking for updates in {}/{}", o, r);
    update_info ui (co_await fetch_latest_release (o, r));

    // We might get an empty result if we filtered out all available releases
    // (e.g., they were all drafts).
    //
    if (ui.empty ())
    {
      launcher::log::debug (categories::update{}, "no valid remote releases found during update check");
      co_return update_info ();
    }

    // Compare what we found with what we are running. Only bother returning
    // the info if it's actually an upgrade.
    //
    if (ui.version > v)
    {
      launcher::log::info (categories::update{}, "update available: target version {} is newer than current", ui.tag_name);
      co_return ui;
    }

    launcher::log::info (categories::update{}, "current version is up to date (remote tag: {})", ui.tag_name);
    co_return update_info ();
  }

  asio::awaitable<update_info> update_discovery::
  fetch_latest_release (const string& o, const string& r)
  {
    launcher::log::trace_l2 (categories::update{}, "fetching latest release metadata from {}/{} (include_prerelease: {})", o, r, include_prerelease_);

    // Grab a batch of releases. We ask for 20 hoping that if we are ignoring
    // prereleases, we will find at least one stable release in this set.
    //
    vector<release_type> rs (
      co_await api_->get_releases (o, r, 20));

    if (rs.empty ())
    {
      launcher::log::warning (categories::update{}, "api returned no releases for {}/{}", o, r);
      co_return update_info ();
    }

    launcher::log::trace_l3 (categories::update{}, "api returned {} releases", rs.size ());

    // Iterate through what we got and pick the winner. Note that the API
    // returns them sorted by creation date (newest first).
    //
    const release_type* b (nullptr);

    for (const auto& i : rs)
    {
      launcher::log::trace_l3 (categories::update{}, "evaluating release tag '{}' (draft: {}, prerelease: {})", i.tag_name, i.draft, i.prerelease);

      if (i.draft)
      {
        launcher::log::trace_l3 (categories::update{}, "skipping draft release {}", i.tag_name);
        continue;
      }

      // If we are okay with prereleases, the first non-draft is the newest.
      // Otherwise, keep digging until we hit a stable one.
      //
      if (include_prerelease_ || !i.prerelease)
      {
        b = &i;
        launcher::log::debug (categories::update{}, "selected release '{}' as latest valid update candidate", b->tag_name);
        break;
      }
      else
      {
        launcher::log::trace_l3 (categories::update{}, "skipping prerelease {}", i.tag_name);
      }
    }

    if (b == nullptr)
    {
      launcher::log::warning (categories::update{}, "no suitable non-draft/non-prerelease found in the latest {} releases", rs.size ());
      co_return update_info ();
    }

    co_return release_to_update_info (*b);
  }

  asio::awaitable<update_info> update_discovery::
  fetch_release_by_tag (const string& o,
                        const string& r,
                        const string& t)
  {
    launcher::log::trace_l2 (categories::update{}, "fetching specific release by tag '{}' from {}/{}", t, o, r);

    release_type rel (
      co_await api_->get_release_by_tag (o, r, t));

    if (rel.empty ())
    {
      launcher::log::warning (categories::update{}, "release with tag '{}' not found", t);
      co_return update_info ();
    }

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
    launcher::log::trace_l3 (categories::update{}, "converting release '{}' to update_info", r.tag_name);
    update_info ui;

    // Before we bother parsing versions, let's make sure there is actually a
    // binary for us to download. If not, this release is useless to the
    // current platform.
    //
    auto a (find_platform_asset (r));
    if (!a)
    {
      launcher::log::warning (categories::update{}, "no compatible platform asset found in release '{}'", r.tag_name);
      return ui;
    }

    launcher::log::trace_l3 (categories::update{}, "found platform asset: {}", a->name);

    // While the release tag (e.g., "v1.2.3") is useful, the asset name
    // usually contains the full, canonical version (including snapshot IDs,
    // e.g., "1.2.3-a.1-20260201010251.fe4660334ed0"). Try to extract that
    // first.
    //
    auto v (parse_asset_version (a->name));
    if (!v)
    {
      launcher::log::trace_l3 (categories::update{}, "failed to parse version from asset name, falling back to tag name '{}'", r.tag_name);
      // The asset naming scheme might have changed or be non-standard. Fall
      // back to the git tag.
      //
      v = parse_launcher_version (r.tag_name);
      if (!v)
      {
        launcher::log::error (categories::update{}, "failed to parse launcher version from tag name '{}'", r.tag_name);
        return ui;
      }
    }

    launcher::log::debug (categories::update{}, "resolved update info for version (from tag: {})", r.tag_name);

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
    {
      launcher::log::error (categories::update{}, "current platform is unknown, cannot find appropriate asset");
      return nullopt;
    }

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
      launcher::log::trace_l3 (categories::update{}, "searching for windows_x64 asset (*{}.*)", s);
      break;
    case platform_type::linux_x64:
      s = "x86_64-linux-glibc";
      e = ".tar.xz";
      launcher::log::trace_l3 (categories::update{}, "searching for linux_x64 asset (*{}.*)", s);
      break;
    default:
      launcher::log::warning (categories::update{}, "unsupported platform_type: {}", static_cast<int> (p));
      return nullopt;
    }

    // Scan the assets. We look for the standard pattern:
    // launcher-<version>-<platform>.<ext>
    //
    for (const auto& a : r.assets)
    {
      launcher::log::trace_l3 (categories::update{}, "checking asset: {}", a.name);
      if (a.name.find (s) != string::npos &&
          a.name.find (e) != string::npos &&
          a.name.find ("launcher-") == 0)
      {
        launcher::log::debug (categories::update{}, "found matching platform asset: {}", a.name);
        return a;
      }
    }

    launcher::log::warning (categories::update{}, "no asset matched the expected platform pattern (prefix 'launcher-', suffix '{}', ext '{}')", s, e);
    return nullopt;
  }

  optional<launcher_version> update_discovery::
  parse_asset_version (const string& n) const
  {
    launcher::log::trace_l3 (categories::update{}, "attempting to parse version from asset name: {}", n);

    // The plan is to strip the known prefix ("launcher-") and the known
    // platform suffix. Whatever is left in the middle should be the version.
    //
    const string p ("launcher-");
    if (n.find (p) != 0)
    {
      launcher::log::trace_l3 (categories::update{}, "asset name '{}' does not start with '{}'", n, p);
      return nullopt;
    }

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
        launcher::log::trace_l3 (categories::update{}, "matched platform suffix '{}' at pos {}", s, e);
        break;
      }
    }

    // Sanity check: suffix must appears after the prefix.
    //
    if (e == string::npos || e <= b)
    {
      launcher::log::trace_l3 (categories::update{}, "no valid platform suffix found in asset name '{}'", n);
      return nullopt;
    }

    string s (n.substr (b, e - b));
    launcher::log::trace_l3 (categories::update{}, "extracted version string '{}' from asset name", s);

    auto res (parse_launcher_version (s));
    if (!res)
      launcher::log::warning (categories::update{}, "failed to parse '{}' into a valid launcher_version", s);

    return res;
  }
}
