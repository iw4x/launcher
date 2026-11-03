// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef _WIN32
#  include <winsock2.h>
#  include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/process/v2/process.hpp>
#include <boost/process/v2/start_dir.hpp>
#include <boost/process/v2/environment.hpp>

#include <launcher/arch/arch.hxx>

#include <launcher/launcher-cache.hxx>
#include <launcher/launcher-connect-protocol.hxx>
#include <launcher/launcher-download.hxx>
#include <launcher/launcher-github.hxx>
#include <launcher/launcher-http.hxx>
#include <launcher/launcher-installation.hxx>
#include <launcher/launcher-log.hxx>
#include <launcher/launcher-manifest.hxx>
#include <launcher/launcher-options.hxx>
#include <launcher/launcher-progress.hxx>
#include <launcher/launcher-shortcut.hxx>
#include <launcher/launcher-update.hxx>
#include <launcher/launcher-utility.hxx>

#ifdef __linux__
#  include <launcher/launcher-steam-proton.hxx>
#endif

#include <launcher/version.hxx>

namespace bp = boost::process::v2;
namespace asio = boost::asio;
namespace views = std::views;
namespace ranges = std::ranges;

using namespace std;
using namespace std::filesystem;
using namespace boost::asio::experimental::awaitable_operators;

namespace launcher
{
  namespace
  {
    // GitHub repositories of the component releases and the CDN that serves
    // the DLC.
    //
    constexpr const char* github_org          = "iw4x";
    constexpr const char* client_repo         = "iw4x-client";
    constexpr const char* rawfiles_repo       = "iw4x-rawfiles";
    constexpr const char* steam_helper_repo   = "launcher-steam";
    constexpr const char* cdn_manifest_url    = "https://cdn.iw4x.io/update.json";
    constexpr const char* cdn_base_url        = "https://cdn.iw4x.io/";

    constexpr const char* client_x64_org      = "iw4x-x64";
    constexpr const char* client_x64_repo     = "Releases";
    constexpr const char* assets_x64_repo     = "assets";

    // Steam app id of Call of Duty: Modern Warfare 2.
    //
    constexpr std::uint32_t steam_app_id = 10190;

    // Log in the launcher category.
    //
    constexpr auto
    info ([] (auto&&... args)
    {
      log::info (categories::launcher (),
                 std::forward<decltype (args)> (args)...);
    });

    constexpr auto
    trace_l2 ([] (auto&&... args)
    {
      log::trace_l2 (categories::launcher (),
                     std::forward<decltype (args)> (args)...);
    });

    constexpr auto
    error ([] (auto&&... args)
    {
      log::error (categories::launcher (),
                  std::forward<decltype (args)> (args)...);
    });

    constexpr auto
    warning ([] (auto&&... args)
    {
      log::warning (categories::launcher (),
                  std::forward<decltype (args)> (args)...);
    });

    // Return the path of the running executable.
    //
    path
    self_path ()
    {
      error_code ec;

#ifdef _WIN32
      vector<wchar_t> b (32768);
      DWORD n (GetModuleFileNameW (nullptr,
                                   b.data (),
                                   static_cast<DWORD> (b.size ())));

      if (n == 0)
        throw system_error (GetLastError (),
                            system_category (),
                            "GetModuleFileNameW failed");

      path p (wstring_view (b.data (), n));
#else
      path p (canonical ("/proc/self/exe", ec));

      if (ec)
        throw system_error (ec, "failed to resolve /proc/self/exe");
#endif

      if (p.empty ())
        throw runtime_error ("executable path resolved to an empty string");

      return p;
    }

    // Change the working directory to the directory of the running
    // executable, which is the installation root.
    //
    void
    reanchor_cwd ()
    {
      error_code ec;

      path d (self_path ().parent_path ());
      current_path (d, ec);

      if (ec)
        throw system_error (ec,
                            "failed to change working directory to: " +
                              to_utf8 (d));

      trace_l2 ("changed working directory to: {}", to_utf8 (d));
    }

    // Show the wait for the GitHub rate limit to reset in a dialog, starting
    // the progress display for it if it isn't running and stopping it once
    // the wait is over. Note that the update coordinator makes its requests
    // through its discovery.
    //
    template <typename Coordinator>
    void
    bind_rate_limit_ui (asio::io_context& io_ctx,
                        Coordinator& coord,
                        progress_coordinator& progress_coord)
    {
      auto is_ui_active (make_shared<bool> (false));

      auto callback (
        [&io_ctx, &progress_coord, is_ui_active] (const string& message,
                                                  uint64_t remaining_time)
      {
        if (!progress_coord.running ())
        {
          progress_coord.start ();
          *is_ui_active = true;
        }

        string display_str (message + "\n\nTime remaining: " +
                            std::to_string (remaining_time) + " seconds");

        progress_coord.show_dialog ("Rate Limit", display_str);

        if (remaining_time == 0)
        {
          progress_coord.hide_dialog ();

          if (*is_ui_active)
          {
            asio::co_spawn (io_ctx, progress_coord.stop (), asio::detached);
            *is_ui_active = false;
          }
        }
      });

      if constexpr (requires (decltype (callback) func) {
                      coord.discovery ().set_progress_callback (func);
                    })
        coord.discovery ().set_progress_callback (callback);
      else
        coord.set_progress_callback (callback);
    }

    // Return the client that the command line selects. Without one, prompt
    // the user, assuming the x86 client if there is no terminal to prompt
    // on. Return nullopt if the user cancels the prompt.
    //
    optional<architecture>
    resolve_architecture (const options& opt)
    {
      if (opt.arch_specified ())
      {
        info ("client selected on the command line: {}",
              to_string (opt.arch ()));

        return opt.arch ();
      }

      if (!interactive ())
      {
        warning ("no terminal to prompt on, assuming the {} client",
                 to_string (architecture::x86));

        return architecture::x86;
      }

      optional<architecture> a (prompt_architecture ());

      if (a)
        info ("client selected at the prompt: {}", to_string (*a));
      else
        info ("launch cancelled at the client prompt");

      return a;
    }

    // Return true if launching the architecture's client requires the
    // component. Note that the DLC only adds maps.
    //
    bool
    required (architecture a, component_type c)
    {
#ifdef __linux__
      if (c == component_type::helper)
        return true;
#endif

      switch (a)
      {
      case architecture::x86:
        return c == component_type::client || c == component_type::rawfiles;

      case architecture::x64:
        return c == component_type::client_x64 ||
               c == component_type::assets_x64;
      }

      return false;
    }

    // Component that we could not update, with the failure.
    //
    struct update_failure
    {
      component_type component;
      string what;
      exception_ptr error;
    };
  }

  // Check for a newer launcher release, considering the prereleases if p is
  // true, and install it, restarting into it with the arguments. Return true
  // if the new launcher was started and this process should exit. If o
  // (--self-update-only) is true, then a failed check is an error.
  //
  asio::awaitable<bool>
  check_self_update (asio::io_context& io,
                      bool p,
                      bool o,
                      progress_coordinator& pc,
                      vector<string> args)
  {
    info ("checking for launcher updates (prerelease: {}, update_only: {})",
          p,
          o);

    auto uc (make_update_coordinator (io));
    uc->set_include_prerelease (p);
    uc->set_auto_restart (o);

    bind_rate_limit_ui (io, *uc, pc);

    auto s (co_await uc->check_for_updates ());

    if (s == update_status::check_failed && o)
      throw runtime_error ("only update requested but check failed");

    if (s == update_status::up_to_date)
    {
      info ("launcher is up to date");
      co_return false;
    }

    info ("launcher update available, proceeding with installation");

    uc->set_progress_coordinator (&pc);
    pc.start ();

    const auto& i (uc->last_update_info ());

    auto r (co_await uc->install_update (i));

    if (!r.success)
      throw runtime_error ("update failed to install: " + r.error_message);

    info ("restarting into new launcher version");

    // Pass our arguments on so that the new launcher starts the same client
    // (e.g., with the --arch of a shortcut).
    //
    if (!uc->restart (args))
    {
      warning ("failed to restart into new launcher version, continuing "
               "with this one");
      co_return false;
    }

    co_return true;
  }

  // Download the files of the plan and put them in place, recording them in
  // the cache. Each download goes to the staging directory first. Once all
  // of them are complete, we extract the downloaded archives of the manifest
  // into the installation root and move the other files to their
  // destinations. Throw std::runtime_error if a download fails three times.
  //
  asio::awaitable<void>
  execute_plan (asio::io_context& io,
                download_coordinator& dc,
                progress_coordinator& pc,
                cache_coordinator& cc,
                vector<reconcile_item> pl,
                manifest md,
                path ir)
  {
    struct dl_info
    {
      string         url;
      path           tmp;
      path           dst;
      uint64_t       size;
      component_type comp;
      string         ver;
      string         hash;
      int            retries;
      bool           done;
    };

    error_code e;
    installation ins (ir);

    path sd (weakly_canonical (ins.cache () / "staging", e));

    if (e)
      throw system_error (e,
                          "failed to canonicalize staging directory: " +
                            to_utf8 (sd));

    create_directories (sd, e);

    if (e)
      throw system_error (e,
                          "failed to create staging directory: " +
                            to_utf8 (sd));

    // A staged file is named after the digest of its destination as well,
    // which keeps apart the files of the same name from different
    // directories.
    //
    auto is_dl ([] (const auto& i)
    {
      return i.action == reconcile_action::download;
    });

    auto to_dl ([&sd] (const auto& i)
    {
      if (i.url.empty ())
        throw runtime_error (
          "reconciliation failed: missing download URL for " + i.path);

      path d (from_utf8 (i.path));
      path t (sd / (path_digest (d) + "_" + to_utf8 (d.filename ())));

      return dl_info {i.url,
                      t,
                      d,
                      i.expected_size,
                      i.component,
                      i.version,
                      i.expected_hash,
                      0,
                      false};
    });

    vector<dl_info> ds;
    for (auto&& x : pl | views::filter (is_dl) | views::transform (to_dl))
      ds.push_back (std::move (x));

    if (ds.empty ())
      co_return;

    struct active_task
    {
      shared_ptr<download_coordinator::task_type> h;
      shared_ptr<progress_entry> ui;
      dl_info* ctx;

      active_task (shared_ptr<download_coordinator::task_type> h_,
                   shared_ptr<progress_entry> ui_,
                   dl_info* ctx_)
        : h (std::move (h_)),
          ui (std::move (ui_)),
          ctx (ctx_)
      {
      }
    };

    // Download in batches, each with the files that are not yet downloaded
    // and have attempts left. Connections drop often enough during large
    // downloads that we give each file three attempts.
    //
    auto is_pd ([] (const auto& d)
    {
      return !d.done && d.retries < 3;
    });

    bool hp (ranges::any_of (ds, is_pd));

    while (hp)
    {
      vector<active_task> ts;

      auto pds (ds | views::filter (is_pd));

      for (auto& d : pds)
      {
        download_request r;
        r.urls.push_back (d.url);
        r.target = d.tmp;
        r.name = to_utf8 (d.dst.filename ());
        r.expected_size = d.size;

        auto en (pc.add_entry (r.name));
        auto t (dc.queue_download (std::move (r)));

        en->metrics ().total_bytes.store (d.size, memory_order_relaxed);

        t->on_progress = [en, &pc] (const download_progress& dp)
        {
          pc.update_progress (en, dp.downloaded_bytes, dp.total_bytes);
        };

        ts.emplace_back (t, en, &d);
      }

      if (ts.empty ())
        break;

      info ("executing download batch ({} queued files)", ts.size ());
      pc.start ();

      auto ml ([&io, &ts, &pc] () -> asio::awaitable<void>
      {
        // Wait for the tasks to finish, removing their progress entries and
        // marking each file downloaded or counting its failed attempt.
        //
        asio::steady_timer tm (io);
        while (!ts.empty ())
        {
          erase_if (ts,
                    [&pc] (const auto& at)
          {
            if (at.h->completed () || at.h->failed ())
            {
              at.h->on_progress = nullptr;
              pc.remove_entry (at.ui);

              if (at.h->completed ())
                at.ctx->done = true;
              else
                at.ctx->retries++;

              return true;
            }

            return false;
          });

          if (!ts.empty ())
          {
            tm.expires_after (chrono::milliseconds (25));
            co_await tm.async_wait (asio::use_awaitable);
          }
        }
      });

      // Run the downloads and the wait together. Note that with
      // wait_for_all() neither keeps running if the other throws.
      //
      auto [order, err_dl, err_ui](
        co_await asio::experimental::make_parallel_group (
          asio::co_spawn (io, dc.execute_all (), asio::deferred),
          asio::co_spawn (io, ml (), asio::deferred))
          .async_wait (asio::experimental::wait_for_all (),
                       asio::use_awaitable));

      if (err_dl)
        std::rethrow_exception (err_dl);
      if (err_ui)
        std::rethrow_exception (err_ui);

      co_await pc.stop ();

      hp = ranges::any_of (ds, is_pd);
    }

    auto is_f ([] (const auto& d)
    {
      return !d.done;
    });

    ptrdiff_t fc (ranges::count_if (ds, is_f));

    if (fc > 0)
      throw runtime_error (std::to_string (fc) +
                           " downloads failed permanently after retries");

    info ("validating and applying staged files...");

    unordered_map<string, const manifest_archive*> am;

    for (const auto& a : md.archives)
      am[to_utf8 (manifest_coordinator::resolve_path (a, ir))] = &a;

    auto archive ([&am] (const dl_info& d) -> const manifest_archive*
    {
      if (d.dst.extension () != ".zip" && d.dst.extension () != ".ZIP")
        return nullptr;

      auto i (am.find (to_utf8 (d.dst)));
      return i != am.end () ? i->second : nullptr;
    });

    for (const auto& d : ds)
    {
      // A missing staged file or one of an unexpected size means that the
      // download was truncated or that writing it failed.
      //
      if (!exists (d.tmp, e))
        throw runtime_error ("downloaded file missing from staging area: " +
                             to_utf8 (d.tmp));

      uintmax_t fs (file_size (d.tmp, e));

      if (e || fs == 0 || (d.size > 0 && fs != d.size))
      {
        throw runtime_error ("downloaded file failed size validation: " +
                             to_utf8 (d.tmp));
      }

      if (archive (d) != nullptr)
        continue;

      if (d.dst.has_parent_path ())
      {
        path pd (weakly_canonical (d.dst.parent_path (), e));

        if (e)
          throw system_error (e,
                              "failed to canonicalize parent directory: " +
                                to_utf8 (pd));

        create_directories (pd, e);

        if (e)
          throw system_error (e,
                              "failed to create parent directory: " +
                                to_utf8 (pd));
      }

      // If the existing file is in use, then move it out of the way. Without
      // this both the rename and the copy below would fail.
      //
      if (exists (d.dst) && !remove (d.dst, e))
        ins.retire (d.dst);

      rename (d.tmp, d.dst, e);

      // The rename fails if the destination is on another filesystem, in
      // which case we copy the file.
      //
      if (e)
      {
        copy_file (d.tmp, d.dst, copy_options::overwrite_existing, e);

        if (!e)
          remove (d.tmp, e);
        else
          throw runtime_error ("failed to apply file: " + to_utf8 (d.dst));
      }
    }

    for (const auto& d : ds)
    {
      // We record the files that an archive installs and remove the archive
      // itself.
      //
      if (const manifest_archive* a = archive (d))
      {
        info ("extracting downloaded archive: {}", to_utf8 (d.tmp));

        vector<path> efs (
          co_await manifest_coordinator::extract_archive (*a, d.tmp, ir));

        cc.track (efs, d.comp, d.ver);
        remove (d.tmp, e);

        continue;
      }

      cc.track (to_utf8 (d.dst), d.comp, d.ver, d.hash);
    }
  }

  // Bring the IW4x client up to date with its latest release. If the cache
  // records the release and the files match their records, then we skip the
  // planning. We look up the release asset of a file without a URL by its
  // file name.
  //
  asio::awaitable<void>
  sync_client (asio::io_context& io,
                github_coordinator& gh,
                download_coordinator& dc,
                progress_coordinator& pc,
                cache_coordinator& cc,
                path root,
                bool pre)
  {
    info ("synchronizing client component...");

    auto rel (co_await gh.fetch_latest_release (github_org, client_repo, pre));
    bool out (cc.outdated (component_type::client, rel.tag_name));

    if (!out)
    {
      auto s (cc.audit (component_type::client));
      bool ok (ranges::all_of (s | views::values, [] (auto st) {
        return st == file_state::valid; }));

      if (ok)
      {
        info ("client components are valid and up to date");
        co_return;
      }

      warning ("client physical audit failed, forcing reconcile");
    }

    auto ms (co_await gh.fetch_manifest (rel));
    manifest m (ms);
    auto p (cc.plan (m, component_type::client, rel.tag_name));

    for (auto& i : p | views::filter ([] (const auto& x) {
      return x.action == reconcile_action::download && x.url.empty (); }))
    {
      string fn (to_utf8 (from_utf8 (i.path).filename ()));
      auto it (ranges::find_if (rel.assets, [&fn] (const auto& a) {
        return a.name == fn; }));

      if (it != rel.assets.end ())
        i.url = it->browser_download_url;

      if (i.url.empty ())
      {
        throw runtime_error (
          "reconciliation failed: unable to resolve URL for " +
          i.path);
      }
    }

    co_await execute_plan (io, dc, pc, cc, p, m, root);
    cc.clean (m, component_type::client);
    cc.stamp (component_type::client, rel.tag_name);
  }

  // Remove the files that a release with another tag installed for the
  // component, along with their records.
  //
  void
  prune_component (cache_coordinator& cc,
                   component_type comp,
                   const string& what,
                   const string& tag)
  {
    for (const auto& f : cc.database ().files (comp))
    {
      if (f.version () == tag)
        continue;

      path p (from_utf8 (f.path ()));

      error_code ec;
      remove (p, ec);

      if (ec)
        warning ("failed to remove stale {} file {}: {}",
                 what,
                 f.path (),
                 to_utf8_system_message (ec.message ()));
      else
        trace_l2 ("removed stale {} file: {}", what, f.path ());

      cc.forget (p);
    }
  }

  // Bring up to date the component that is released as a single zip
  // archive, which we extract into the installation root with the files
  // excluded and renamed as specified.
  //
  asio::awaitable<void>
  sync_archive_release (asio::io_context& io,
                        github_coordinator& gh,
                        download_coordinator& dc,
                        progress_coordinator& pc,
                        cache_coordinator& cc,
                        path root,
                        bool pre,
                        component_type comp,
                        string what,
                        string org,
                        string repo,
                        vector<string> exclude = {},
                        vector<pair<string, string>> rename = {})
  {
    info ("synchronizing {} component...", what);

    auto rel (co_await gh.fetch_latest_release (org, repo, pre));

    bool out (cc.outdated (comp, rel.tag_name));

    if (!out)
    {
      auto s (cc.audit (comp));

      bool ok (!s.empty () &&
               ranges::all_of (s | views::values, [] (auto st) {
                 return st == file_state::valid; }));

      if (ok)
      {
        info ("{} components are valid and up to date", what);
        co_return;
      }

      warning ("{} physical audit failed, forcing reconcile", what);
    }

    auto it (ranges::find_if (rel.assets, [] (const auto& a)
    {
      return from_utf8 (a.name).extension () == ".zip";
    }));

    if (it == rel.assets.end ())
      throw runtime_error (what + " release " + rel.tag_name +
                           " carries no archive asset");

    manifest m;

    manifest_archive a;

    a.name = it->name;
    a.url = it->browser_download_url;
    a.size = it->size;
    a.exclude = std::move (exclude);
    a.rename = std::move (rename);

    m.archives.push_back (std::move (a));

    auto p (cc.plan (m, comp, rel.tag_name));

    co_await execute_plan (io, dc, pc, cc, p, m, root);

    prune_component (cc, comp, what, rel.tag_name);
    cc.stamp (comp, rel.tag_name);
  }

  // Bring the IW4x (mm) client up to date. We install its iw4mp.exe under
  // the client's name and leave out its Unlinker.exe.
  //
  asio::awaitable<void>
  sync_client_x64 (asio::io_context& io,
                   github_coordinator& gh,
                   download_coordinator& dc,
                   progress_coordinator& pc,
                   cache_coordinator& cc,
                   path root,
                   bool pre)
  {
    // Note that older GCC versions fail to compile braced initializer
    // arguments in a co_await expression.
    //
    vector<string> exclude {"Unlinker.exe"};

    vector<pair<string, string>> rename {
      {"iw4mp.exe", string (architecture_executable (architecture::x64))}};

    co_await sync_archive_release (io, gh, dc, pc, cc, root, pre,
                                   component_type::client_x64,
                                   "IW4x (mm) client",
                                   client_x64_org,
                                   client_x64_repo,
                                   std::move (exclude),
                                   std::move (rename));
  }

  // Bring the fastfiles of the IW4x (mm) client up to date. The release
  // archive holds zone/iw4x/x64/ as installed.
  //
  asio::awaitable<void>
  sync_assets_x64 (asio::io_context& io,
                   github_coordinator& gh,
                   download_coordinator& dc,
                   progress_coordinator& pc,
                   cache_coordinator& cc,
                   path root,
                   bool pre)
  {
    co_await sync_archive_release (io, gh, dc, pc, cc, root, pre,
                                   component_type::assets_x64,
                                   "IW4x (mm) assets",
                                   client_x64_org,
                                   assets_x64_repo);
  }

  // Bring the rawfiles up to date with their latest release. As for the
  // client, we skip the planning if the recorded release and files are
  // current.
  //
  asio::awaitable<void>
  sync_rawfiles (asio::io_context& io,
                  github_coordinator& gh,
                  download_coordinator& dc,
                  progress_coordinator& pc,
                  cache_coordinator& cc,
                  path root,
                  bool pre)
  {
    info ("synchronizing rawfiles component...");

    auto rel (co_await gh.fetch_latest_release (github_org, rawfiles_repo, pre));
    bool out (cc.outdated (component_type::rawfiles, rel.tag_name));

    if (!out)
    {
      auto s (cc.audit (component_type::rawfiles));
      bool ok (ranges::all_of (s | views::values, [] (auto st) {
        return st == file_state::valid; }));

      if (ok)
      {
        info ("rawfiles components are valid and up to date");
        co_return;
      }

      warning ("rawfiles physical audit failed, forcing reconcile");
    }

    auto ms (co_await gh.fetch_manifest (rel));
    manifest m (ms);

    // The archive keeps its asset URL (see fetch_manifest()) and we download
    // it as release.zip, the name that the release also publishes it under.
    //
    for (auto& a : m.archives)
    {
      if (a.name == "__launcher_archive.zip")
        a.name = "release.zip";
    }

    auto p (cc.plan (m, component_type::rawfiles, rel.tag_name));

    unordered_map<string, const manifest_file*> fm;

    for (const auto& f : m.files)
      fm.emplace (to_utf8 (manifest_coordinator::resolve_path (f, root)), &f);

    // Look up the release asset of a file without a URL by the file's asset
    // name and then by its file name. The release publishes the other files
    // as __launcher_<name>.bin, with the dots of the name replaced (e.g.,
    // __launcher_iw4x_00_iwd.bin).
    //
    for (auto& i : p | views::filter ([] (const auto& x) {
      return x.action == reconcile_action::download && x.url.empty (); }))
    {
      auto fi (fm.find (i.path));

      string fn (fi != fm.end ()
                 ? to_utf8 (from_utf8 (fi->second->path).filename ())
                 : to_utf8 (from_utf8 (i.path).filename ()));

      string an (fi != fm.end () && fi->second->asset_name
                 ? *fi->second->asset_name
                 : string ());

      auto by_name ([&rel] (const string& n)
      {
        return ranges::find_if (rel.assets, [&n] (const auto& a)
        {
          return a.name == n;
        });
      });

      auto it (an.empty () ? rel.assets.end () : by_name (an));

      if (it == rel.assets.end ())
        it = by_name (fn);

      if (it == rel.assets.end ())
      {
        string mn (fn);
        replace (mn.begin (), mn.end (), '.', '_');

        it = by_name ("__launcher_" + mn + ".bin");
      }

      if (it != rel.assets.end ())
        i.url = it->browser_download_url;

      if (i.url.empty ())
      {
        throw runtime_error (
          "reconciliation failed: unable to resolve URL for " +
          i.path);
      }
    }

    co_await execute_plan (io, dc, pc, cc, p, m, root);
    cc.clean (m, component_type::rawfiles);
    cc.stamp (component_type::rawfiles, rel.tag_name);
  }

  // Bring the DLC files up to date with the manifest on the CDN, which also
  // serves the files. Note that the DLC has no releases, so we record it
  // under the dlc version.
  //
  asio::awaitable<void>
  sync_dlc (asio::io_context& io,
            http_coordinator& hc,
            download_coordinator& dc,
            progress_coordinator& pc,
            cache_coordinator& cc,
            path root)
  {
    info ("synchronizing dlc component...");

    auto ms (co_await hc.get (cdn_manifest_url));
    manifest dlc (ms, manifest_format::dlc);

    manifest m;
    for (const auto& f : dlc.files)
    {
      manifest_archive x;
      x.name = f.path;
      x.url = string (cdn_base_url) + f.path;
      x.size = f.size;
      x.hash = f.hash;

      m.archives.push_back (std::move (x));
    }

    // Take over the copies that earlier launchers installed into zone/dlc/
    // before the planning, which then adopts those that are current (see
    // installation::migrate_dlc()).
    //
    {
      vector<path> ts;

      for (const auto& a : m.archives)
        ts.push_back (manifest_coordinator::resolve_path (a, root));

      installation (root).migrate_dlc (ts);
    }

    auto p (cc.plan (m, component_type::dlc, "dlc"));

    for (auto& i : p | views::filter ([] (const auto& x) {
      return x.action == reconcile_action::download && x.url.empty (); }))
    {
      auto it (ranges::find_if (m.archives,
                                [&i, &root] (const auto& a)
      {
        return to_utf8 (from_utf8 (i.path)) ==
          to_utf8 (manifest_coordinator::resolve_path (a, root));
      }));

      if (it != m.archives.end ())
        i.url = it->url;

      if (i.url.empty ())
      {
        throw runtime_error (
          "reconciliation failed: unable to resolve URL for " + i.path);
      }
    }

    co_await execute_plan (io, dc, pc, cc, p, m, root);
    cc.clean (m, component_type::dlc);
    cc.stamp (component_type::dlc, "dlc");
  }

#ifdef __linux__
  // Bring our Steam helper, which the Proton launch runs (see
  // proton_coordinator::setup_for_launch()), up to date with its latest
  // release.
  //
  asio::awaitable<void>
  sync_helper (asio::io_context& io,
                github_coordinator& gh,
                download_coordinator& dc,
                progress_coordinator& pc,
                cache_coordinator& cc,
                path root,
                bool pre)
  {
    info ("synchronizing linux steam helper component...");

    auto rel (co_await gh.fetch_latest_release (github_org, steam_helper_repo, pre));
    bool out (cc.outdated (component_type::helper, rel.tag_name));

    if (!out)
    {
      auto s (cc.audit (component_type::helper));
      bool ok (ranges::all_of (s | views::values, [] (auto st) {
        return st == file_state::valid; }));

      if (ok)
      {
        info ("steam helper components are valid and up to date");
        co_return;
      }

      warning ("steam helper physical audit failed, forcing reconcile");
    }

    manifest m;
    for (const auto& a : rel.assets)
    {
      if (a.name != "steam.exe" && a.name != "steam_api64.dll")
        continue;

      manifest_archive x;
      x.name = a.name;
      x.url = a.browser_download_url;
      x.size = a.size;

      m.archives.push_back (std::move (x));
    }

    auto p (cc.plan (m, component_type::helper, rel.tag_name));

    for (auto& i : p | views::filter ([] (const auto& x) {
      return x.action == reconcile_action::download && x.url.empty (); }))
    {
      string fn (to_utf8 (from_utf8 (i.path).filename ()));
      auto it (ranges::find_if (rel.assets, [&fn] (const auto& a) {
        return a.name == fn; }));

      if (it != rel.assets.end ())
        i.url = it->browser_download_url;

      if (i.url.empty ())
        throw runtime_error (
          "reconciliation failed: unable to resolve URL for " +
          i.path);
    }

    co_await execute_plan (io, dc, pc, cc, p, m, root);
    cc.clean (m, component_type::helper);
    cc.stamp (component_type::helper, rel.tag_name);
  }
#endif

  // Start the client executable in the installation root, passing it the
  // arguments. On Linux we launch it through Proton and fall back to Wine.
  //
#ifdef __linux__
  asio::awaitable<void>
  execute (asio::io_context& io,
            path root,
            string exe,
            vector<string> args,
            const bool force_steam_runtime)
  {
    if (exe.empty ())
      throw runtime_error ("game binary unspecified");

    path f (root / from_utf8 (exe));
    error_code ec;

    path bin (canonical (f, ec));

    if (ec)
      throw system_error (ec,
                          "failed to canonicalize game binary path: " +
                            to_utf8 (f));

    proton_coordinator proton (io, force_steam_runtime);

    if (!exists (root / "steam.exe"))
      throw runtime_error ("runtime dependency missing: steam.exe");

    path steam_root;

    if (const char* h = getenv ("HOME"))
      steam_root = path (h) / ".steam" / "steam";

    info ("launching {} via proton", to_utf8 (bin));

    bool ok (co_await proton.complete_launch (steam_root, bin, steam_app_id, args));

    if (!ok)
    {
      warning ("proton execution failed, attempting fallback to wine");

      path wine_exe (bp::environment::find_executable ("wine").native ());

      if (wine_exe.empty ())
        throw runtime_error ("wine is not installed or not in PATH");

      path p (canonical (wine_exe, ec));

      if (ec)
        throw system_error (ec,
                            "failed to canonicalize wine executable path: " +
                              to_utf8 (wine_exe));

      wine_exe = move (p);

      vector<string> wa;
      wa.push_back (to_utf8 (bin));
      wa.insert (wa.end (), args.begin (), args.end ());

      info ("launching via wine: {} {}", to_utf8 (wine_exe), to_utf8 (bin));

      bp::process (io,
                   bp::filesystem::path (wine_exe.native ()),
                   wa,
                   bp::process_start_dir (root.native ())).detach ();

      info ("wine game process spawned");
    }
    else
    {
      info ("proton execution initiated");
    }
  }
#else
  asio::awaitable<void>
  execute (asio::io_context& io,
            path root,
            string exe,
            vector<string> args,
            bool /* force_steam_runtime */)
  {
    if (exe.empty ())
      throw runtime_error ("game binary unspecified");

    path f (root / from_utf8 (exe));
    error_code ec;

    path bin (canonical (f, ec));

    if (ec)
      throw system_error (ec,
                          "failed to canonicalize game binary path: " +
                            to_utf8 (f));

    info ("launching native game process: {}", to_utf8 (bin));

    // Note that Boost.Process converts the arguments from UTF-8 for
    // CreateProcessW(), so they can represent any path and argument (e.g.,
    // Cyrillic on a Western installation).
    //
    bp::process (io,
                 bp::filesystem::path (bin.native ()),
                 args,
                 bp::process_start_dir (root.native ())).detach ();

    info ("native game process spawned");

    co_return;
  }
#endif
}

using namespace launcher;

int
main (int argc, char* argv[])
try
{
  options opt (argc, argv);

  // Handle --build2-metadata (see also buildfile).
  //
  if (opt.build2_metadata_specified ())
  {
    auto& o (cout);

    // Note that the export.metadata variable should be the first non-
    // blank/comment line.
    //
    o << "# build2 buildfile iw4x-launcher" << "\n"
      << "export.metadata = 1 launcher" << "\n"
      << "launcher.name = [string] iw4x-launcher" << "\n"
      << "launcher.version = [string] '" << LAUNCHER_VERSION_FULL << "'\n"
      << "launcher.checksum = [string] '" << LAUNCHER_VERSION_FULL << "'\n";

    return 0;
  }

  // Handle --version.
  //
  if (opt.version ())
  {
    auto& o (cout);

    o << "Launcher " << LAUNCHER_VERSION_ID << "\n";

    return 0;
  }

  // Handle --help.
  //
  if (opt.help ())
  {
    auto& o (cout);

    o << "usage: launcher [options]" << "\n"
      << "options:" << "\n";

    opt.print_usage (o);

    return 0;
  }

  // Give the process that keeps our files locked some time to exit (see
  // --wait-pid).
  //
#ifdef _WIN32
  if (opt.wait_pid_specified () && !opt.wait_pid ().empty ())
  {
    unsigned long pid (std::strtoul (opt.wait_pid ().c_str (), nullptr, 10));
    if (pid != 0)
    {
      if (HANDLE h = OpenProcess (SYNCHRONIZE, FALSE, pid))
      {
        WaitForSingleObject (h, 30000);
        CloseHandle (h);
      }
    }
  }
#endif

  reanchor_cwd ();

  {
    error_code ec;
    create_directories (path ("cache"), ec);
  }

  active_logger = new logger;

  // The installation root is the working directory (see reanchor_cwd()).
  //
  path cwd (current_path ());

  error_code root_ec;

  path root (canonical (cwd, root_ec));

  if (root_ec)
    throw system_error (root_ec,
                        "failed to canonicalize installation root: " +
                          to_utf8 (cwd));

  info ("installation root: {}", to_utf8 (root));

  installation ins (root);
  ins.remove_retired ();

  asio::io_context io;

  if ((!opt.no_self_update () || opt.self_update_only ()) && !opt.skip_remote ())
  {
    progress_coordinator pc (io);
    exception_ptr ex;
    bool restarted (false);

    vector<string> args (argv + 1, argv + argc);

    asio::co_spawn (io,
                    [&io, &opt, &pc, &args, &restarted] ()
                      -> asio::awaitable<void>
    {
      exception_ptr ep;

      try
      {
        restarted = co_await check_self_update (io,
                                                opt.prerelease (),
                                                opt.self_update_only (),
                                                pc,
                                                args);
      }
      catch (...)
      {
        ep = current_exception ();
      }

      co_await pc.stop ();

      if (ep)
        rethrow_exception (ep);

    } (), [&ex] (exception_ptr ep) { ex = ep; });

    io.restart ();
    io.run ();
    io.restart ();

    if (ex)
      rethrow_exception (ex);

    // The new launcher carries on from here.
    //
    if (restarted || opt.self_update_only ())
      return 0;
  }

  // Start afresh if the cache belongs to another launcher version.
  //
  ins.reset_cache (LAUNCHER_VERSION_ID);

  if (!opt.no_shortcuts ())
  {
    shortcut_coordinator sc (self_path (), root);
    sc.refresh ();
  }

  ins.migrate_clients ();

  // Updating each component requires its release, which GitHub and the CDN
  // serve. If they are unavailable, then we can still launch an installed
  // client, so we carry on past the components that we could not update and
  // decide once we know the client (see below).
  //
  vector<update_failure> failures;

  if (!opt.skip_remote ())
  {
    http_client_traits ht;
    if (!opt.proxy ().empty ())
      ht.proxy_url = opt.proxy ();

    github_coordinator   gh (io);
    http_coordinator     hc (io, ht);
    download_coordinator dc (io, opt.jobs (), ht);
    progress_coordinator pc (io);
    cache_coordinator    cc (root);

    if (!opt.proxy ().empty ())
      gh.set_proxy (opt.proxy ());

    cc.set_github_coordinator (&gh);
    cc.set_download_coordinator (&dc);
    cc.set_progress_coordinator (&pc);

    // Show the wait if the GitHub requests hit the rate limit.
    //
    bind_rate_limit_ui (io, gh, pc);

    exception_ptr sync_ex;

    asio::co_spawn (
      io,
      [&io, &gh, &hc, &dc, &pc, &cc, &root, &opt, &failures] ()
        -> asio::awaitable<void>
    {
      // Update the component, recording the failure. Note that a failed
      // download can leave the progress display running.
      //
      auto sync ([&pc, &failures] (component_type c,
                                   string what,
                                   asio::awaitable<void> a)
                   -> asio::awaitable<void>
      {
        try
        {
          co_await std::move (a);
          co_return;
        }
        catch (const exception& e)
        {
          warning ("unable to update {}: {}",
                   what,
                   to_utf8_system_message (e.what ()));

          failures.push_back ({c, std::move (what), current_exception ()});
        }

        co_await pc.stop ();
      });

      co_await sync (component_type::client,
                     "client",
                     sync_client (io, gh, dc, pc, cc, root, opt.prerelease ()));

      co_await sync (component_type::client_x64,
                     "IW4x (mm) client",
                     sync_client_x64 (io, gh, dc, pc, cc, root, true));

      co_await sync (component_type::assets_x64,
                     "IW4x (mm) assets",
                     sync_assets_x64 (io, gh, dc, pc, cc, root, true));

      co_await sync (
        component_type::rawfiles,
        "rawfiles",
        sync_rawfiles (io, gh, dc, pc, cc, root, opt.prerelease ()));

      co_await sync (component_type::dlc,
                     "dlc",
                     sync_dlc (io, hc, dc, pc, cc, root));

#ifdef __linux__
      co_await sync (component_type::helper,
                     "steam helper",
                     sync_helper (io, gh, dc, pc, cc, root, true));
#endif
    }(),
      [&io, &sync_ex] (exception_ptr ep)
    {
      sync_ex = ep;
      io.stop ();
    });

    io.restart ();
    io.run ();

    if (sync_ex)
      rethrow_exception (sync_ex);

    // Without its update the DLC that we have installed still has to be
    // safe from its legacy copies, which we can check with what the cache
    // records (see installation::migrate_dlc()).
    //
    if (ranges::any_of (failures, [] (const update_failure& f)
    {
      return f.component == component_type::dlc;
    }))
    {
      vector<path> fs;

      for (const auto& f : cc.database ().files (component_type::dlc))
        fs.push_back (from_utf8 (f.path ()));

      ins.migrate_dlc (fs);
    }

    if (failures.empty ())
      info ("all components synchronized and up to date");
  }
  else
  {
    info ("skipping remote checks and reconciliation (--skip-remote)");
  }

  if (!opt.no_connect_protocol ())
  {
    refresh_connect_protocol (
      root / from_utf8 (architecture_executable (architecture::x86)));
  }

  // Without the launch the updates are all there is, so a failed one is an
  // error.
  //
  if (opt.skip_launch ())
  {
    if (!failures.empty ())
      rethrow_exception (failures.front ().error);

    info ("updates completed, skipping game launch as requested and exiting");
    return 0;
  }

  string exe (opt.game_exe ());

  if (exe.empty ())
  {
    optional<architecture> arch (resolve_architecture (opt));

    if (!arch)
      return 0;

    exe = architecture_executable (*arch);

    // Launch the installed version of each component that the client
    // requires and that we could not update. Note that we don't know what a
    // client specified with --game-exe requires.
    //
    if (!failures.empty ())
    {
      cache_coordinator cc (root);

      for (const update_failure& f : failures)
      {
        if (!required (*arch, f.component))
          continue;

        if (!cc.installed (f.component))
        {
          error ("{} is not installed and could not be updated",
                 f.what);
          rethrow_exception (f.error);
        }

        warning ("launching the installed {} since it could not be updated",
                 f.what);
      }
    }
  }

  exception_ptr exec_ex;

  asio::co_spawn (
    io,
    [&io, &root, &opt, &exe] () -> asio::awaitable<void>
  {
    co_await execute (
        io,
        root,
        exe,
        opt.game_args (),
        opt.force_steam_runtime ());

  } (), [&io, &exec_ex] (exception_ptr ep) { exec_ex = ep; io.stop (); });

  io.restart ();
  io.run ();

  if (exec_ex)
    rethrow_exception (exec_ex);

  info ("execution payload dispatched, terminating launcher");

  return 0;
}
catch (const cli::exception& ex)
{
  cerr << "error: " << ex << endl;
  return 1;
}
catch (const exception& ex)
{
  error ("exception caught in main: {}",
         to_utf8_system_message (ex.what ()));
  error ("please report this to the IW4x developers on Discord: "
         "https://discord.com/invite/2ETE8engZM");
  return 1;
}
catch (...)
{
  error ("You've run into a launcher bug we didn't see coming. Since this "
         "isn't something a restart can fix, please report it to our team so "
         "we can investigate.");
  error ("please report this to the IW4x developers on Discord: "
         "https://discord.com/invite/2ETE8engZM");
  return 1;
}
