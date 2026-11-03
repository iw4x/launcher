// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include <launcher/cache/cache.hxx>

#include <launcher/launcher-download.hxx>
#include <launcher/launcher-github.hxx>
#include <launcher/launcher-progress.hxx>
#include <launcher/manifest/manifest.hxx>

namespace launcher
{
  namespace asio = boost::asio;
  namespace fs = std::filesystem;

  // Outcome of bringing a component up to date.
  //
  enum class cache_status
  {
    up_to_date,       // Nothing to do.
    update_required,  // Planned but not applied (see check()).
    update_applied,   // Brought up to date.
    check_failed,     // Planning failed.
    update_failed     // Applying failed (e.g., a download failed).
  };

  std::ostream&
  operator<< (std::ostream&, cache_status);

  // Status of bringing a component up to date along with the summary of
  // its plan. A failure that we expect, such as a failed download, comes
  // with its description in the error member.
  //
  struct cache_result
  {
    cache_status status;
    reconcile_summary summary;
    std::string error;

    cache_result ();

    explicit
    cache_result (cache_status s);

    cache_result (cache_status s, reconcile_summary sum);

    cache_result (cache_status s, std::string e);

    // Return true if the component is up to date, possibly after the update.
    //
    bool
    ok () const noexcept;

    explicit operator bool () const noexcept;
  };

  // Installation files of the components, brought up to date with their
  // release manifests through the reconciler and the downloads.
  //
  // Note that the coordinators that we are given must outlive us.
  //
  class cache_coordinator
  {
  public:
    using database_type = cache_database;
    using reconciler_type = reconciler;

    // Called as the planning hashes the files, with the message and the
    // number of the files hashed so far and in total.
    //
    using progress_callback =
      std::function<void (const std::string& msg,
                          std::size_t cur,
                          std::size_t tot)>;

    // Open the cache database of the installation directory, creating it if
    // it doesn't exist.
    //
    cache_coordinator (asio::io_context& ctx, const fs::path& dir);

    cache_coordinator (const cache_coordinator&) = delete;
    cache_coordinator& operator= (const cache_coordinator&) = delete;

    // Configuration.
    //

    // Set the coordinator of the downloads, which updating requires.
    //
    void
    set_download_coordinator (download_coordinator* dl);

    // Set the coordinator that shows the progress of the downloads. Without
    // one we show nothing.
    //
    void
    set_progress_coordinator (progress_coordinator* p);

    void
    set_github_coordinator (github_coordinator* gh);

    void
    set_progress_callback (progress_callback cb);

    // Set how we compare a file with its record (see strategy).
    //
    void
    set_strategy (strategy s);

    strategy
    get_strategy () const noexcept;

    // Versions.
    //

    // Return true if the recorded version of the component is not the tag
    // (see reconciler::outdated()). Note that this doesn't look at the files.
    //
    bool
    outdated (component_type c, const std::string& tag) const;

    // Return the recorded version of the component, if any.
    //
    std::optional<std::string>
    version (component_type c) const;

    // Inspection.
    //

    // Return the state of the file relative to its record.
    //
    file_state
    stat (const fs::path& p) const;

    // Return the state of every recorded file of the component.
    //
    std::vector<std::pair<cached_file, file_state>>
    audit (component_type c) const;

    // Return true if the component is installed, that is, it is recorded at
    // a version and all its recorded files, of which there is at least one,
    // match their records. This is what we can find out about the component
    // without its release.
    //
    bool
    installed (component_type c) const;

    // Planning.
    //

    // Return the plan that brings the component up to date with the
    // manifest at the version (see reconciler::plan()).
    //
    std::vector<reconcile_item>
    plan (const manifest& m,
          component_type c,
          const std::string& v);

    reconcile_summary
    summarize (const std::vector<reconcile_item>& items) const;

    // Plan the update of the component and return update_required if there
    // is anything to do, without changing the installation.
    //
    cache_result
    check (const manifest& m,
           component_type c,
           const std::string& v);

    // Updating.
    //

    // Plan the update of the component and apply it, downloading the files
    // and recording them along with the version.
    //
    asio::awaitable<cache_result>
    sync (const manifest& m,
          component_type c,
          const std::string& v);

    // As above but if the component is recorded at the tag and all its
    // files match their records, then return up_to_date without planning,
    // which saves hashing the files.
    //
    asio::awaitable<cache_result>
    smart_sync (const manifest& m,
                component_type c,
                const std::string& tag);

    // As sync() for several components at once, with all their downloads in
    // one batch. Note that the downloaded files are not recorded, only the
    // versions.
    //
    asio::awaitable<cache_result>
    sync_all (
      const std::vector<std::tuple<manifest,
                                   component_type,
                                   std::string>>& items);

    // Recording (see the reconciler functions of the same names).
    //

    void
    track (const fs::path& p,
           component_type c,
           const std::string& v,
           const std::string& h = "");

    void
    track (const std::vector<fs::path>& ps,
           component_type c,
           const std::string& v);

    void
    stamp (component_type c, const std::string& tag);

    void
    forget (const fs::path& p);

    // Return the records of the component's files that are no longer in the
    // manifest (see reconciler::clean()). Note that the files themselves are
    // left alone.
    //
    std::vector<std::string>
    clean (const manifest& m, component_type c);

    // Database maintenance.
    //

    // Erase every record.
    //
    void
    clear ();

    void
    vacuum ();

    // Return true if the database passes the SQLite integrity check.
    //
    bool
    check_integrity () const;

    database_type&
    database () noexcept;

    const database_type&
    database () const noexcept;

    reconciler_type&
    get_reconciler () noexcept;

    const reconciler_type&
    get_reconciler () const noexcept;

    const fs::path&
    install_directory () const noexcept;

  private:
    // Download the files of the plan, recording each one that we download,
    // and on success record the version of the component.
    //
    asio::awaitable<cache_result>
    execute (const std::vector<reconcile_item>& items,
             component_type c,
             const std::string& v);

    asio::io_context& ctx_;
    fs::path dir_;

    database_type db_;
    reconciler_type rec_;

    download_coordinator* dl_ = nullptr;
    progress_coordinator* prog_ = nullptr;
    github_coordinator* gh_ = nullptr;

    progress_callback cb_;
  };
}
