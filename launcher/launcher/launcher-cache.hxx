#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include <launcher/cache/cache.hxx>

#include <launcher/manifest/manifest.hxx>

namespace launcher
{
  namespace asio = boost::asio;
  namespace fs = std::filesystem;

  // The outcome of a cache reconciliation attempt.
  //
  // We distinguish between up_to_date (we checked and everything is fine) and
  // update_applied (we had to move bits). If we failed to verify the state,
  // we return check_failed rather than throwing, as this is often a
  // recoverable state in the UI.
  //
  enum class cache_status
  {
    up_to_date,       // Disk state verified matches manifest/tag.
    update_required,  // Diff calculated, download/copy pending.
    update_applied,   // We mutated the disk, now in sync.
    check_failed,     // IO or logic error during verify/plan.
    update_failed     // IO or network error during execution.
  };

  std::ostream&
  operator<< (std::ostream&, cache_status);

  // We need to bundle the status with the summary so the caller knows not
  // just "what happened" (status) but "what changed" (summary).
  //
  // We also carry the error string here to avoid throwing exceptions across
  // coroutine boundaries for "expected" runtime failures (like a 404).
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

    // Returns true if we are in a consistent state. Note that up_to_date is
    // considered "ok", as is a successful update.
    //
    bool
    ok () const noexcept;

    explicit operator bool () const noexcept;
  };

  class download_coordinator;
  class progress_coordinator;
  class github_coordinator;

  // The central coordinator for local asset management.
  //
  // We sit between the raw database (ODB), the filesystem, and the network
  // (download_coordinator). Our primary job is reconciliation: making the
  // disk look like the manifest.
  //
  // We try to keep this class stateless regarding the "process" of updating
  // (that state lives in the coroutine stack), but we do hold references to
  // the external coordinators which must outlive us.
  //
  class cache_coordinator
  {
  public:
    using database_type = cache_database;
    using reconciler_type = reconciler;

    using progress_callback =
      std::function<void (const std::string& msg,
                          std::size_t cur,
                          std::size_t tot)>;

    // We expect the install_dir to be roughly valid, though we will create
    // the .iw4x subdirectory and the sqlite database if they don't exist.
    //
    // Note that we take the io_context by ref as we don't own the thread
    // pool, we just schedule work onto it.
    //
    cache_coordinator (asio::io_context& ctx, const fs::path& dir);

    cache_coordinator (const cache_coordinator&) = delete;
    cache_coordinator& operator= (const cache_coordinator&) = delete;

    // Wiring.
    //

    // We need the downloader to fetch blobs if the plan determines we are
    // missing files.
    //
    void
    set_download_coordinator (download_coordinator* dl);

    // If set, we push TUI updates here. If null, we run silent.
    //
    void
    set_progress_coordinator (progress_coordinator* p);

    // Required for the smart_sync metadata check (resolving tags to
    // manifests) before we hit the heavy local logic.
    //
    void
    set_github_coordinator (github_coordinator* gh);

    void
    set_progress_callback (progress_callback cb);

    // How we decide what to keep when the manifest conflicts with the
    // database. Default is usually 'keep_newer'.
    //
    void
    set_strategy (strategy s);

    strategy
    get_strategy () const noexcept;

    // Queries.
    //

    // Check if the semantic version in the DB differs from the remote tag.
    //
    // This is a cheap metadata-only check. We don't touch the filesystem or
    // verify file hashes here. Use this for "UI badges", not for integrity
    // guarantees.
    //
    bool
    outdated (component_type c, const std::string& tag) const;

    // Return the version string we stored last time we successfully synced or
    // stamped this component.
    //
    std::optional<std::string>
    version (component_type c) const;

    // Inspection.
    //

    // Wrapper around fs::status but aware of our internal file_state logic
    // (e.g., handling permissions/executable bits if we care).
    //
    file_state
    stat (const fs::path& p) const;

    // Walk the database for this component and stat every single file.
    //
    // Warning: This involves O(N) IO operations where N is the file count.
    //
    std::vector<std::pair<cached_file, file_state>>
    audit (component_type c) const;

    // Planning.
    //

    // Diff the manifest against the database/filesystem. This produces the
    // list of actions (keep, download, delete) but executes nothing.
    //
    std::vector<reconcile_item>
    plan (const manifest& m,
          component_type c,
          const std::string& v);

    // Reduce the item list into a statistical summary (bytes to download,
    // files to delete, etc).
    //
    reconcile_summary
    summarize (const std::vector<reconcile_item>& items) const;

    // Run the plan generation but stop short of execution.
    //
    // This effectively answers: "If I were to sync now, what would happen?"
    // without side effects.
    //
    cache_result
    check (const manifest& m,
           component_type c,
           const std::string& v);

    // Execution.
    //

    // The brute-force synchronization.
    //
    // We calculate the plan and immediately execute it. This handles the full
    // lifecycle: download missing, delete orphaned, copy cached.
    //
    asio::awaitable<cache_result>
    sync (const manifest& m,
          component_type c,
          const std::string& v);

    // The "Happy Path" synchronization.
    //
    asio::awaitable<cache_result>
    smart_sync (const manifest& m,
                component_type c,
                const std::string& tag);

    // Batch processor.
    //
    // We chain the coroutines here to avoid the overhead of spinning up
    // multiple execution contexts.
    //
    asio::awaitable<cache_result>
    sync_all (
      const std::vector<std::tuple<manifest,
                                   component_type,
                                   std::string>>& items);

    // Database / Tracking.
    //

    // Register a file we just landed. We update the DB entry to match the
    // current state of the file on disk (mtime, size) so the next quick-check
    // passes.
    //
    void
    track (const fs::path& p,
           component_type c,
           const std::string& v,
           const std::string& h = "");

    // Batch tracking for archive extraction (avoids transaction thrashing).
    //
    void
    track (const std::vector<fs::path>& ps,
           component_type c,
           const std::string& v);

    // Explicitly set the version string for a component.
    //
    // We usually do this after a successful sync, but we might also do it if
    // we detect an external update (e.g. user manually patched).
    //
    void
    stamp (component_type c, const std::string& tag);

    // Remove file from DB tracking. Does not delete from disk.
    //
    void
    forget (const fs::path& p);

    // Maintenance.
    //

    // Sweep files.
    //
    // We look for files in the component's directory that are *not* in the
    // manifest and delete them.
    //
    // Note: Be careful with the strategy here as we don't want to wipe user
    // configs.
    //
    std::vector<std::string>
    clean (const manifest& m, component_type c);

    // Wipe the DB tables. Used during "Repair" or "Reset".
    //
    void
    clear ();

    // Run SQLite VACUUM.
    //
    void
    vacuum ();

    // Run a PRAGMA integrity_check.
    //
    // We should probably run this on startup if we suspect a crash occurred
    // previously.
    //
    bool
    check_integrity () const;

    // Accessors.
    //

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
    // Actually run the IO operations defined in the items list.
    //
    // This is where we hand off to the download_coordinator for fetches and
    // the reconciler for copies/deletes.
    //
    asio::awaitable<cache_result>
    execute (const std::vector<reconcile_item>& items,
             component_type c,
             const std::string& v);

    asio::io_context& ctx_;
    fs::path dir_;

    std::unique_ptr<database_type> db_;
    std::unique_ptr<reconciler_type> rec_;

    download_coordinator* dl_ = nullptr;
    progress_coordinator* prog_ = nullptr;
    github_coordinator* gh_ = nullptr;

    progress_callback cb_;
  };
}
