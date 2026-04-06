#pragma once

#include <boost/asio/awaitable.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <launcher/cache/cache-database.hxx>
#include <launcher/cache/cache-types.hxx>
#include <launcher/launcher-log.hxx>
#include <launcher/manifest/manifest.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // Reconciliation strictness.
  //
  // We offer a spectrum of paranoia: from believing the OS file timestamps
  // (fast) to verifying content hashes (slow but safe).
  //
  enum class strategy
  {
    mtime, // Trust mtime.
    mixed, // Check mtime and size.
    hash   // Verify content hash.
  };

  // Filesystem versus database versus manifest synchronizer.
  //
  // The idea here is to determine the minimum set of actions required to make
  // the filesystem look like the manifest.
  //
  class reconciler
  {
  public:
    using progress_cb =
      std::function<void (const std::string& msg, size_t cur, size_t tot)>;

    // By default, we trust the filesystem.
    //
    static constexpr strategy def_strat = strategy::mtime;

    // Whether to automatically prune orphaned files.
    //
    static constexpr bool auto_prune = true;

    // We borrow the database reference, so it must outlive us. Note that we
    // also hold onto the root install path for all resolutions.
    //
    reconciler (cache_database& db, const fs::path& root);

    reconciler (const reconciler&) = delete;
    reconciler& operator= (const reconciler&) = delete;

    // Configuration.
    //

    // We allow hot-swapping the strategy, though usually this is determined at
    // startup.
    //
    void
    mode (strategy s);

    strategy
    mode () const noexcept;

    void
    progress (progress_cb cb);

    // Versioning.
    //

    // Check if the component is stale compared to the tag.
    //
    // We assume the database is the source of truth for what is currently
    // installed. If the database says we have version X, and the remote is X,
    // we return false (up-to-date) and assume the user hasn't corrupted files
    // manually.
    //
    bool
    outdated (component_type c, const std::string& tag) const;

    // Peek at the database version. Return nullopt if the component has never
    // been installed.
    //
    std::optional<std::string>
    version (component_type c) const;

    // Inspection.
    //

    // Stat a single file.
    //
    // If the entry is provided, we compare the filesystem state against it
    // using our current mode. If it is missing, we assume the file is untracked
    // or new.
    //
    file_state
    stat (const fs::path& p) const;

    file_state
    stat (const fs::path& p, const cached_file& entry) const;

    // Walk the entire database for this component and check every file against
    // the filesystem. This is the heavy check we run if versions mismatch or if
    // the user forced a verify.
    //
    std::vector<std::pair<cached_file, file_state>>
    audit (component_type c) const;

    // Planning.
    //

    // Generate the to-do list.
    //
    // We iterate over the manifest. For every file or archive, we look it up in
    // the database and check the filesystem. If anything is amiss (missing,
    // modified, wrong version), we add a reconcile item.
    //
    std::vector<reconcile_item>
    plan (const manifest& m, component_type c, const std::string& v);

    // Helpers for the planner. We split these out to keep the logic manageable
    // and to handle the slightly different semantics of archives (which need
    // extraction) versus standalone files.
    //
    std::vector<reconcile_item>
    plan_archives (const std::vector<manifest_archive>& as,
                   component_type c,
                   const std::string& v);

    std::vector<reconcile_item>
    plan_files (const std::vector<manifest_file>& fs,
                component_type c,
                const std::string& v);

    reconcile_summary
    summarize (const std::vector<reconcile_item>& items) const;

    // Recording.
    //

    // Commit a download to the database.
    //
    // We do this immediately after download (before extraction) so we don't
    // re-download if the process crashes during extraction.
    //
    void
    track (const fs::path& p,
           component_type c,
           const std::string& v,
           const std::string& hash = "");

    // Commit extracted files.
    //
    // We batch this because an archive might explode into thousands of files.
    // Touching the database that many times is too slow.
    //
    void
    track (const std::vector<fs::path>& ps,
           component_type c,
           const std::string& v);

    // Finalize the update.
    //
    // Once the dust settles and all files are essentially correct, we stamp the
    // component with the new version tag.
    //
    void
    stamp (component_type c, const std::string& tag);

    void
    forget (const fs::path& p);

    // Garbage collection.
    //
    // Scan the database for files belonging to our component but which are no
    // longer in the manifest. Delete them from both.
    //
    std::vector<std::string>
    clean (const manifest& m, component_type c);

    // Resolution.
    //

    // Anchor the manifest relative path to our root.
    //
    fs::path
    path (const manifest_file& p) const;

    fs::path
    path (const manifest_archive& a) const;

    // Normalize the path to a string key for database lookups. We need to be
    // careful about slashes here to ensure consistency across platforms.
    //
    std::string
    key (const fs::path& p) const;

    // Access.
    //

    cache_database&
    database () noexcept;

    const cache_database&
    database () const noexcept;

    const fs::path&
    root () const noexcept;

  private:
    void
    report (const std::string& msg, size_t cur, size_t tot);

    // The actual comparison logic.
    //
    // If we are in mtime mode, we just check timestamps. If mixed, we check
    // size too. If hash, we read the whole file.
    //
    bool
    match (const fs::path& p, const cached_file& entry) const;

    cache_database& db_;
    fs::path root_;
    strategy strat_;
    progress_cb cb_;
  };
}
