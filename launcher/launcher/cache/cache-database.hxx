#pragma once

#include <launcher/cache/cache-types.hxx>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <odb/database.hxx>
#include <odb/transaction.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/sqlite/database.hxx>

#include <launcher/launcher-log.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // Main database handle.
  //
  // Note that ODB handles connection pooling internally so we just hold the
  // pointer.
  //
  class cache_database
  {
  public:
    // Hide the database inside the cache directory so we don't clutter the
    // user's game root.
    //
    static constexpr const char* db_name = "iw4x.db";
    static constexpr const char* dir_name = "cache";

    // Start schema version at 1.
    //
    // If the cached_file object changes, we bump this. ODB can handle
    // migration, but since this is just a cache, we might also consider a
    // wipe-and-rebuild strategy on mismatch instead of complex migrations.
    //
    static constexpr unsigned int schema_ver = 1;

    // If the database file is missing, we want ODB to generate the schema for
    // us immediately.
    //
    static constexpr bool auto_create = true;

    // SQLite defaults to a rollback journal which blocks readers during
    // writes. Since the game (reader) and launcher (writer) run concurrently,
    // WAL is mandatory here.
    //
    static constexpr bool wal = true;

    explicit
    cache_database (const fs::path& root);

    cache_database (const cache_database&) = delete;
    cache_database& operator= (const cache_database&) = delete;

    ~cache_database ();

    // Check if the unique_ptr is actually holding an active database.
    //
    bool
    open () const noexcept;

    const fs::path&
    path () const noexcept;

    // Accessors for the raw ODB handle.
    //
    // These are useful if we need to run custom queries outside the helpers
    // defined below.
    //
    odb::sqlite::database&
    db () noexcept;

    const odb::sqlite::database&
    db () const noexcept;

    //
    // File queries.
    //

    // Lookup by either the database string key or a filesystem path. The path
    // version simply converts to a string before querying.
    //
    std::optional<cached_file>
    find (const std::string& p) const;

    std::optional<cached_file>
    find (const fs::path& p) const;

    // Upsert the file. If it already exists in the database, ODB updates it.
    // Otherwise, it inserts a new record.
    //
    void
    store (const cached_file& f);

    // Batch store optimization.
    //
    // We wrap this in a single transaction in the implementation to avoid the
    // overhead of committing on a per-file basis.
    //
    void
    store (const std::vector<cached_file>& fs);

    void
    erase (const std::string& p);

    void
    erase (const std::vector<std::string>& ps);

    // Erase all files belonging to a specific component.
    //
    void
    erase (component_type c);

    //
    // Bulk retrievals.
    //

    std::vector<cached_file>
    files () const;

    std::vector<cached_file>
    files (component_type c) const;

    std::vector<cached_file>
    files (const std::string& v) const;

    //
    // Stats.
    //

    std::size_t
    count () const;

    std::size_t
    count (component_type c) const;

    //
    // Version tracking.
    //

    // Track the installed version of each component separately.
    //
    // This allows us to know if an update is needed without the overhead of
    // hashing every single file.
    //
    std::optional<component_version>
    version (component_type c) const;

    void
    version (component_type c, const std::string& tag);

    void
    erase_version (component_type c);

    std::vector<component_version>
    versions () const;

    // User settings.
    //

    std::optional<user_setting>
    setting (const std::string& key) const;

    std::string
    setting_value (const std::string& key) const;

    void
    setting (const std::string& key, const std::string& value);

    void
    erase_setting (const std::string& key);

    // Transaction wrappers.
    //

    // Generic transaction wrapper.
    //
    // If the functor throws, we let the transaction destructor rollback. If
    // it succeeds, we commit.
    //
    template <typename F>
    void
    transact (F&& f)
    {
      launcher::log::trace_l3 (categories::cache{}, "executing generic transaction");
      odb::transaction t (db_->begin ());
      f ();
      t.commit ();
    }

    template <typename F>
    auto
    transact_r (F&& f) -> decltype (f ())
    {
      launcher::log::trace_l3 (categories::cache{}, "executing generic transaction with return value");
      odb::transaction t (db_->begin ());
      auto r (f ());
      t.commit ();
      return r;
    }

    //
    // Maintenance.
    //

    // Reclaim filesystem space.
    //
    // Note that SQLite doesn't return filesystem space automatically on
    // delete, so we have to explicitly ask for it via vacuum.
    //
    void
    vacuum ();

    // Run 'PRAGMA integrity_check'.
    //
    // This is a good idea to run on startup if we suspect a crash occurred
    // previously.
    //
    bool
    check () const;

    // Drop all tables and data.
    //
    void
    clear ();

  private:
    void
    init (const fs::path& root);

    // Check schema_catalog.
    //
    // If the table doesn't exist, we create it. If the version doesn't match,
    // we migrate (or fail, depending on how we configure the migration).
    //
    void
    schema ();

    // Setup database pragmas (WAL, synchronous modes, etc).
    //
    void
    pragmas ();

    fs::path path_;
    std::unique_ptr<odb::sqlite::database> db_;
  };
}
