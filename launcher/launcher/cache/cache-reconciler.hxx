// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <boost/asio/awaitable.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <launcher/cache/cache-database.hxx>
#include <launcher/cache/cache-types.hxx>
#include <launcher/launcher-log.hxx>
#include <launcher/manifest/manifest.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // What a file has to match in its record to be considered intact (see
  // reconciler::stat()).
  //
  enum class strategy
  {
    mtime, // Modification time.
    mixed, // Modification time and size.
    hash   // @@ Same as mixed since match() doesn't hash the file yet.
  };

  // Reconciler of an installation with a release manifest.
  //
  // It compares the installed files with their cache records and with the
  // manifest, plans the downloads that bring the installation up to date,
  // and records what gets installed.
  //
  class reconciler
  {
  public:
    // Progress callback, called with the message, the number of items done,
    // and their total.
    //
    using progress_cb =
      std::function<void (const std::string& msg, size_t cur, size_t tot)>;

    static constexpr strategy def_strat = strategy::mtime;

    // Whether clean() also erases the records that it returns.
    //
    static constexpr bool auto_prune = true;

    // Note that the database must outlive the reconciler. The installation
    // root is made canonical.
    //
    reconciler (cache_database& db, const fs::path& root);

    reconciler (const reconciler&) = delete;
    reconciler& operator= (const reconciler&) = delete;

  public:
    // Configuration.
    //

    // Set and return the strategy.
    //
    void
    mode (strategy s);

    strategy
    mode () const noexcept;

    // Set the callback that reports the hashing progress (see plan()).
    //
    void
    progress (progress_cb cb);

  public:
    // Versions.
    //

    // Return true if the component is not installed or its installed version
    // is not the tag. Note that this only looks at the recorded version and
    // not at the files (see audit() for that).
    //
    bool
    outdated (component_type c, const std::string& tag) const;

    // Return the installed version of the component or nullopt if it is not
    // installed.
    //
    std::optional<std::string>
    version (component_type c) const;

  public:
    // Inspection.
    //

    // Return the state of the file relative to its record. The second
    // version takes the record that the caller already has.
    //
    file_state
    stat (const fs::path& p) const;

    file_state
    stat (const fs::path& p, const cached_file& entry) const;

    // Return the state of every file of the component that we have a record
    // of.
    //
    std::vector<std::pair<cached_file, file_state>>
    audit (component_type c) const;

  public:
    // Planning.
    //

    // Return the downloads that bring the files and archives of the
    // component in the manifest up to date with the version.
    //
    // A file that has no up-to-date record but exists and matches the
    // manifest hash is adopted: we record it instead of downloading it
    // again. Note that this involves hashing such files, which we do in
    // parallel, reporting the progress through the callback.
    //
    std::vector<reconcile_item>
    plan (const manifest& m, component_type c, const std::string& v);

    // Records of the component by their paths and the archive and file
    // halves of plan(). Note that an archive with files in the manifest is
    // extracted on installation and so we check it through its files,
    // downloading it again if any of them is not intact.
    //
    using cache_map =
      std::unordered_map<std::string, cached_file>;

    std::vector<reconcile_item>
    plan_archives (const std::vector<manifest_archive>& as,
                   component_type c,
                   const std::string& v,
                   const cache_map& cm);

    std::vector<reconcile_item>
    plan_files (const std::vector<manifest_file>& fs,
                component_type c,
                const std::string& v,
                const cache_map& cm);

    // Return the totals of the plan.
    //
    reconcile_summary
    summarize (const std::vector<reconcile_item>& items) const;

  public:
    // Recording.
    //

    // Record the installed file as belonging to the version of the
    // component, along with its current metadata and, if specified, its
    // hash.
    //
    void
    track (const fs::path& p,
           component_type c,
           const std::string& v,
           const std::string& hash = "");

    // As above but for the files of an extracted archive, which we record in
    // a single transaction since there can be thousands of them.
    //
    void
    track (const std::vector<fs::path>& ps,
           component_type c,
           const std::string& v);

    // Record the tag as the installed version of the component. We do this
    // once all the files of the component are installed.
    //
    void
    stamp (component_type c, const std::string& tag);

    // Erase the record of the file.
    //
    void
    forget (const fs::path& p);

    // Return the records of the component for files that are no longer in
    // the manifest and, if auto_prune is true, erase them. Note that the
    // files themselves are left alone.
    //
    std::vector<std::string>
    clean (const manifest& m, component_type c);

  public:
    // Paths.
    //

    // Return the absolute path of the manifest file or archive in the
    // installation (see manifest_coordinator::resolve_path()).
    //
    fs::path
    path (const manifest_file& p) const;

    fs::path
    path (const manifest_archive& a) const;

    // Return the record key of the file, which is its lexically normalized
    // path.
    //
    std::string
    key (const fs::path& p) const;

  public:
    cache_database&
    database () noexcept;

    const cache_database&
    database () const noexcept;

    const fs::path&
    root () const noexcept;

  private:
    void
    report (const std::string& msg, size_t cur, size_t tot);

    // Return true if the file matches its record according to the strategy.
    // Return false if the file cannot be examined.
    //
    bool
    match (const fs::path& p, const cached_file& entry) const;

    cache_database& db_;
    fs::path root_;
    strategy strat_;
    progress_cb cb_;
  };
}
