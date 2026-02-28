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

  template <typename S = std::string>
  struct cache_database_traits
  {
    using string_type = S;
    using database_type = odb::sqlite::database;

    // We hide the DB inside the .iw4x directory so we don't clutter the
    // user's game root.
    //
    static constexpr const char* db_name = "iw4x.db";
    static constexpr const char* dir_name = ".iw4x";

    // Start at 1. If the cached_file object changes, we bump this. ODB can
    // handle migration, but since this is just a cache, we might also
    // consider a wipe-and-rebuild strategy on mismatch.
    //
    static constexpr unsigned int schema_ver = 1;

    // If the DB file is missing, we want ODB to generate the schema for us
    // immediately.
    //
    static constexpr bool auto_create = true;

    // SQLite defaults to a rollback journal, but that blocks readers during
    // writes. Since the game (reader) and launcher (writer) run concurrently,
    // WAL is mandatory.
    //
    static constexpr bool wal = true;
  };

  // The main database handle.
  //
  // Note that ODB handles connection pooling internally. We just hold the
  // pointer.
  //
  template <typename T = cache_database_traits<>>
  class basic_cache_database
  {
  public:
    using traits_type = T;
    using string_type = typename traits_type::string_type;
    using database_type = typename traits_type::database_type;

    explicit
    basic_cache_database (const fs::path& root);

    basic_cache_database (const basic_cache_database&) = delete;
    basic_cache_database& operator= (const basic_cache_database&) = delete;

    ~basic_cache_database ();

    // Check if the unique_ptr is actually holding a db.
    //
    bool
    open () const noexcept;

    const fs::path&
    path () const noexcept;

    // Accessors for the raw ODB handle if we need to run custom queries
    // outside the helpers defined below.
    //
    database_type&
    db () noexcept;

    const database_type&
    db () const noexcept;

    // File queries.
    //

    // We can lookup by either the DB string key or a filesystem path. The
    // path version just converts to string before querying.
    //
    std::optional<cached_file>
    find (const string_type& p) const;

    std::optional<cached_file>
    find (const fs::path& p) const;

    // Upsert logic. If the file exists, ODB updates it, otherwise it inserts.
    //
    void
    store (const cached_file& f);

    // Batch optimization: we wrap this in a single transaction in the impl to
    // avoid the overhead of committing per-file.
    //
    // @@: consider whether we should commit on a per-file basis?
    //
    void
    store (const std::vector<cached_file>& fs);

    void
    erase (const string_type& p);

    void
    erase (const std::vector<string_type>& ps);

    // Nuke all files belonging to a specific component.
    //
    void
    erase (component_type c);

    // Bulk retrievals.
    //
    std::vector<cached_file>
    files () const;

    std::vector<cached_file>
    files (component_type c) const;

    std::vector<cached_file>
    files (const string_type& v) const;

    // Stats.
    //
    std::size_t
    count () const;

    std::size_t
    count (component_type c) const;

    // Version tracking.
    //

    // We track the installed version of each component separately so we know
    // if an update is needed without hashing every file.
    //
    std::optional<component_version>
    version (component_type c) const;

    void
    version (component_type c, const string_type& tag);

    void
    erase_version (component_type c);

    std::vector<component_version>
    versions () const;

    // User settings.
    //

    std::optional<user_setting>
    setting (const string_type& key) const;

    string_type
    setting_value (const string_type& key) const;

    void
    setting (const string_type& key, const string_type& value);

    void
    erase_setting (const string_type& key);

    // Transaction wrappers.
    //

    // If f() throws, we let the transaction destructor rollback. If it
    // succeeds, we commit.
    //
    template <typename F>
    void
    transact (F&& f);

    template <typename F>
    auto
    transact_r (F&& f) -> decltype (f ());

    // Maintenance.
    //

    // SQLite doesn't return FS space automatically on delete. We have to ask
    // for it.
    //
    void
    vacuum ();

    // Runs 'PRAGMA integrity_check'. Good to run on startup if we suspect a
    // crash occurred previously.
    //
    bool
    check () const;

    // Drop everything.
    //
    void
    clear ();

  private:
    void
    init (const fs::path& root);

    // Checks schema_catalog. If the table doesn't exist, we create it. If the
    // version doesn't match, we migrate (or fail, depending on how we
    // configure the migration).
    //
    void
    schema ();

    // Set WAL, sync modes, etc.
    //
    void
    pragmas ();

    fs::path path_;
    std::unique_ptr<database_type> db_;
  };

  using cache_database = basic_cache_database<>;
}

#include <launcher/cache/cache-database.ixx>
#include <launcher/cache/cache-database.txx>
