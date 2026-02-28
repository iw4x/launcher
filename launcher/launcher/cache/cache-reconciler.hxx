#pragma once

#include <boost/asio/awaitable.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <launcher/cache/cache-database.hxx>
#include <launcher/cache/cache-types.hxx>

#include <launcher/manifest/manifest.hxx>
#include <launcher/launcher-log.hxx>

namespace launcher
{
  namespace asio = boost::asio;
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

  template <typename D = cache_database,
            typename S = std::string>
  struct reconciler_traits
  {
    using db_type = D;
    using str_type = S;

    // By default, we trust the filesystem.
    //
    static constexpr strategy def_strat = strategy::mtime;

    // Whether to automatically prune orphaned files.
    //
    static constexpr bool auto_prune = true;
  };

  // Filesystem vs Database vs Manifest synchronizer.
  //
  // The idea here is to determine the minimum set of actions required to make
  // the filesystem look like the manifest.
  //
  template <typename T = reconciler_traits<>>
  class basic_reconciler
  {
  public:
    using traits = T;
    using db_type = typename traits::db_type;
    using str_type = typename traits::str_type;

    using progress_cb =
      std::function<void (const str_type& msg, size_t cur, size_t tot)>;

    // We borrow the db reference, so it must outlives us. Note that we also
    // hold onto the root install path for all resolution.
    //
    basic_reconciler (db_type& db, const fs::path& root);

    basic_reconciler (const basic_reconciler&) = delete;
    basic_reconciler& operator= (const basic_reconciler&) = delete;

    // Configuration.
    //

    // We allow hot-swapping the strategy, though usually, this is determined
    // at startup.
    //
    // @@: conceptually interesting, but the hot-swap portion may not be worth
    // keeping.
    //
    void
    mode (strategy s);

    strategy
    mode () const noexcept;

    void
    progress (progress_cb cb);

    // Versioning.
    //

    // Check if the component is stale compared to tag.
    //
    // We assume the db is the source of truth for what's currently installed.
    // If the db says we have version X, and remote is X, we return false
    // (up-to-date) and assume the user hasn't corrupted files manually.
    //
    bool
    outdated (component_type c, const str_type& tag) const;

    // Peek at the db version. Returns nullopt if the component has never
    // been installed.
    //
    std::optional<str_type>
    version (component_type c) const;

    // Inspection.
    //

    // Stat a single file.
    //
    // If `entry` is provided, we compare the fs state against it using our
    // current `mode`. If `entry` is missing, we assume the file is untracked
    // or new.
    //
    file_state
    stat (const fs::path& p) const;

    file_state
    stat (const fs::path& p, const cached_file& entry) const;

    // Walk the entire db for this component and check every file against the
    // fs. This is the "heavy" check we run if versions mismatch or if the
    // user forced a verify.
    //
    std::vector<std::pair<cached_file, file_state>>
    audit (component_type c) const;

    // Planning.
    //

    // Generate to-do list.
    //
    // We iterate over the manifest. For every file or archive, we look it up
    // in the db and check the fs. If anything is amiss (missing, modified,
    // wrong version), we add a reconcile item.
    //
    std::vector<reconcile_item>
    plan (const manifest& m, component_type c, const str_type& v);

    // Helpers for the planner. We split these out to keep the logic
    // manageable and to handle the slightly different semantics of archives
    // (which need extraction) vs standalone files.
    //
    std::vector<reconcile_item>
    plan_archives (const std::vector<manifest_archive>& as,
                   component_type c,
                   const str_type& v);

    std::vector<reconcile_item>
    plan_files (const std::vector<manifest_file>& fs,
                component_type c,
                const str_type& v);

    reconcile_summary
    summarize (const std::vector<reconcile_item>& items) const;

    // Recording.
    //

    // Commit a download to the db.
    //
    // We do this immediately after download (before extraction) so we don't
    // re-download if the process crashes during extraction.
    //
    void
    track (const fs::path& p,
           component_type c,
           const str_type& v,
           const str_type& hash = "");

    // Commit extracted files.
    //
    // We batch this because an archive might explode into 1000s of files.
    // touching the db 1000 times is too slow.
    //
    void
    track (const std::vector<fs::path>& ps,
           component_type c,
           const str_type& v);

    // Finalize the update.
    //
    // Once the dust settles and all files are essentially correct, we stamp
    // the component with the new version tag.
    //
    void
    stamp (component_type c, const str_type& tag);

    void
    forget (const fs::path& p);

    // Garbage collection.
    //
    // Scan the db for files belonging to our component but whom are NO LONGER
    // in the manifest. Delete them from both.
    //
    std::vector<str_type>
    clean (const manifest& m, component_type c);

    // Resolution.
    //

    // Anchor the manifest relative path to our `root_`.
    //
    fs::path
    path (const manifest_file& p) const;

    fs::path
    path (const manifest_archive& a) const;

    // Normalize path to a string key for db lookups. We need to be careful
    // about slashes here to ensure consistency across platforms.
    //
    str_type
    key (const fs::path& p) const;

    // Access.
    //

    db_type&
    database () noexcept;

    const db_type&
    database () const noexcept;

    const fs::path&
    root () const noexcept;

  private:
    void
    report (const str_type& msg, size_t cur, size_t tot);

    // The actual comparison logic.
    //
    // If we are in mtime mode, we just check timestamps. If `mixed`, we check
    // size too. If `hash`, we read the whole file. If...
    //
    bool
    match (const fs::path& p, const cached_file& entry) const;

    db_type& db_;
    fs::path root_;
    strategy strat_;
    progress_cb cb_;
  };

  using reconciler = basic_reconciler<>;
}

#include <launcher/cache/cache-reconciler.ixx>
#include <launcher/cache/cache-reconciler.txx>
