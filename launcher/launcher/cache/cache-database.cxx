#include <launcher/cache/cache-database.hxx>

#include <stdexcept>

#include <odb/query.hxx>
#include <odb/result.hxx>

#include <launcher/cache/cache-types-odb.hxx>

using namespace std;

namespace launcher
{
  cache_database::
  cache_database (const fs::path& d)
  {
    // Construct the cache database and initialize the schema and pragmas.
    //
    launcher::log::trace_l2 (categories::cache {},
                             "constructing cache_database");
    init (d);
  }

  cache_database::
  ~cache_database () {}

  bool cache_database::
  open () const noexcept
  {
    // Check if the database is open. We might be called before init() in
    // some flows, so this is useful.
    //
    launcher::log::trace_l3 (categories::cache {},
                             "checking if cache database is open");
    return db_ != nullptr;
  }

  const fs::path& cache_database::
  path () const noexcept
  {
    // Return the resolved path to the cache database.
    //
    launcher::log::trace_l3 (categories::cache {},
                             "retrieving cache database path: {}",
                             path_.string ());
    return path_;
  }

  odb::sqlite::database& cache_database::
  db () noexcept
  {
    // Access the mutable raw database handle.
    //
    launcher::log::trace_l3 (categories::cache {},
                             "accessing mutable raw database handle");
    return *db_;
  }

  const odb::sqlite::database& cache_database::
  db () const noexcept
  {
    // Access the const raw database handle.
    //
    launcher::log::trace_l3 (categories::cache {},
                             "accessing const raw database handle");
    return *db_;
  }

  void cache_database::
  init (const fs::path& d)
  {
    launcher::log::trace_l1 (categories::cache {},
                             "initializing cache database at root: {}",
                             d.string ());

    // Stick the cache database into the standard cache directory layout.
    //
    fs::path c (d / dir_name);

    if (!fs::exists (c))
    {
      // The cache directory does not exist, so create it.
      //
      launcher::log::trace_l2 (categories::cache {},
                               "cache directory {} does not exist, creating it",
                               c.string ());

      error_code e;
      fs::create_directories (c, e);

      if (e)
      {
        launcher::log::error (categories::cache {},
                              "failed to create cache directory: {}",
                              e.message ());
        throw runtime_error ("failed to create cache directory: " +
                             c.string () + ": " + e.message ());
      }
    }

    path_ = c / db_name;
    launcher::log::debug (categories::cache {},
                          "resolved database path to: {}",
                          path_.string ());

    // Check if the file exists before we open the database. This way we
    // know if we need to bootstrap the schema.
    //
    bool cr (!fs::exists (path_));
    if (cr)
      launcher::log::info (categories::cache {},
                           "database file missing, will bootstrap schema");

    // Open with the create flag, which is the default for sqlite::database.
    //
    launcher::log::trace_l2 (categories::cache {}, "opening sqlite database");
    db_ = make_unique<odb::sqlite::database> (path_.string (),
                                              SQLITE_OPEN_READWRITE |
                                                SQLITE_OPEN_CREATE);

    // Tweak the engine before doing anything else.
    //
    launcher::log::trace_l2 (categories::cache {}, "applying database pragmas");
    pragmas ();

    if (cr || auto_create)
    {
      launcher::log::trace_l2 (categories::cache {},
                               "checking/creating database schema");
      schema ();
    }
  }

  void cache_database::
  schema ()
  {
    // Note that ODB's create_schema() fails if the table already exists.
    // Since it doesn't offer a clean "if_not_exists" check, we manually
    // peek at sqlite_master to see if our tables are there.
    //
    bool ex (false);
    {
      odb::transaction t (db_->begin ());

      odb::sqlite::connection& c (
        static_cast<odb::sqlite::connection&> (t.connection ()));

      sqlite3_stmt* s (nullptr);
      const char* q (
        "SELECT name FROM sqlite_master WHERE type='table' AND "
        "name='cached_files'");

      if (sqlite3_prepare_v2 (c.handle (), q, -1, &s, nullptr) == SQLITE_OK)
      {
        if (sqlite3_step (s) == SQLITE_ROW)
          ex = true;

        sqlite3_finalize (s);
      }
      else
        launcher::log::error (categories::cache {},
                              "sqlite3_prepare_v2 failed during schema check");

      t.commit ();
    }

    // Since create_schema() manages its own transaction, the previous check
    // must be fully committed.
    //
    if (!ex)
    {
      launcher::log::info (categories::cache {},
                           "tables missing, telling ODB to create schema");

      odb::transaction t (db_->begin ());
      odb::schema_catalog::create_schema (*db_);
      t.commit ();
    }
    else
      launcher::log::trace_l3 (categories::cache {},
                               "database schema already exists");
  }

  void cache_database::
  pragmas ()
  {
    // Drop down to the raw handle. ODB's execute() enforces a transaction
    // but some pragmas (like WAL and synchronous) require being outside of
    // one.
    //
    odb::connection_ptr c (db_->connection ());
    odb::sqlite::connection& sc (static_cast<odb::sqlite::connection&> (*c));
    sqlite3* h (sc.handle ());

    // Enable WAL mode. This allows better concurrency if we ever have
    // multiple processes, though currently we lock the whole thing.
    //
    if (wal)
    {
      launcher::log::trace_l3 (categories::cache {}, "enabling WAL mode");
      sqlite3_exec (h, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
    }

    // Prioritize performance. If something corrupts, we just rebuild the
    // cache rather than paying a continuous synchronization penalty.
    //
    launcher::log::trace_l3 (
      categories::cache {},
      "disabling synchronous mode and foreign keys, enabling exclusive locking "
      "and memory temp_store");

    sqlite3_exec (h, "PRAGMA synchronous=OFF", nullptr, nullptr, nullptr);
    sqlite3_exec (h, "PRAGMA foreign_keys=OFF", nullptr, nullptr, nullptr);
    sqlite3_exec (h,
                  "PRAGMA locking_mode=EXCLUSIVE",
                  nullptr,
                  nullptr,
                  nullptr);
    sqlite3_exec (h, "PRAGMA temp_store=MEMORY", nullptr, nullptr, nullptr);
  }

  optional<cached_file> cache_database::
  find (const fs::path& p) const
  {
    // Delegate to the string-based find.
    //
    launcher::log::trace_l3 (categories::cache {},
                             "finding file in database by fs::path: {}",
                             p.string ());
    return find (p.string ());
  }

  optional<cached_file> cache_database::
  find (const string& p) const
  {
    launcher::log::trace_l3 (categories::cache {},
                             "querying database for path: {}",
                             p);

    // Wrap the lookup in a short-lived read transaction.
    //
    odb::transaction t (db_->begin ());
    shared_ptr<cached_file> f (db_->find<cached_file> (p));
    t.commit ();

    if (f)
      launcher::log::trace_l3 (categories::cache {},
                               "file found in database: {}",
                               p);
    else
      launcher::log::trace_l3 (categories::cache {},
                               "file not found in database: {}",
                               p);

    return f ? optional<cached_file> (*f) : nullopt;
  }

  void cache_database::
  store (const cached_file& f)
  {
    launcher::log::trace_l3 (categories::cache {},
                             "storing file in database: {}",
                             f.path ());

    odb::transaction t (db_->begin ());

    // We cannot strictly use persist() because the file might already exist.
    // Avoid relying on exceptions for control flow.
    //
    shared_ptr<cached_file> e (db_->find<cached_file> (f.path ()));

    if (e)
    {
      launcher::log::trace_l3 (categories::cache {},
                               "updating existing entry for: {}",
                               f.path ());
      e->set_mtime (f.mtime ());
      e->set_version (f.version ());
      e->set_size (f.size ());
      e->set_hash (f.hash ());
      db_->update (*e);
    }
    else
    {
      launcher::log::trace_l3 (categories::cache {},
                               "persisting new entry for: {}",
                               f.path ());
      db_->persist (f);
    }

    t.commit ();
  }

  void cache_database::
  store (const vector<cached_file>& fs)
  {
    if (fs.empty ())
      return;

    launcher::log::trace_l2 (categories::cache {},
                             "batch storing {} files in database",
                             fs.size ());

    // Batch updates inside a single transaction. This minimizes
    // synchronization overhead.
    //
    odb::transaction t (db_->begin ());

    for (const auto& f : fs)
    {
      shared_ptr<cached_file> e (db_->find<cached_file> (f.path ()));

      if (e)
      {
        e->set_mtime (f.mtime ());
        e->set_version (f.version ());
        e->set_size (f.size ());
        e->set_hash (f.hash ());
        db_->update (*e);
      }
      else
        db_->persist (f);
    }

    t.commit ();
    launcher::log::trace_l2 (categories::cache {}, "batch store complete");
  }

  void cache_database::
  erase (const string& p)
  {
    launcher::log::trace_l2 (categories::cache {},
                             "erasing file from database: {}",
                             p);

    odb::transaction t (db_->begin ());
    db_->erase<cached_file> (p);
    t.commit ();
  }

  void cache_database::
  erase (const vector<string>& ps)
  {
    if (ps.empty ())
      return;

    launcher::log::trace_l2 (categories::cache {},
                             "batch erasing {} files from database",
                             ps.size ());

    odb::transaction t (db_->begin ());

    for (const auto& p : ps)
      db_->erase<cached_file> (p);

    t.commit ();
  }

  void cache_database::
  erase (component_type c)
  {
    launcher::log::info (categories::cache {},
                         "erasing all files for component {} from database",
                         static_cast<int> (c));

    using query = odb::query<cached_file>;

    odb::transaction t (db_->begin ());
    db_->erase_query<cached_file> (query::component == c);
    t.commit ();
  }

  vector<cached_file> cache_database::
  files () const
  {
    launcher::log::trace_l2 (categories::cache {}, "querying all cached files");

    vector<cached_file> r;

    odb::transaction t (db_->begin ());
    odb::result<cached_file> rs (db_->query<cached_file> ());

    for (auto& f : rs)
      r.push_back (f);

    t.commit ();

    launcher::log::trace_l3 (categories::cache {},
                             "query returned {} files",
                             r.size ());
    return r;
  }

  vector<cached_file> cache_database::
  files (component_type c) const
  {
    launcher::log::trace_l2 (categories::cache {},
                             "querying all cached files for component {}",
                             static_cast<int> (c));

    using query = odb::query<cached_file>;
    vector<cached_file> r;

    odb::transaction t (db_->begin ());
    odb::result<cached_file> rs (
      db_->query<cached_file> (query::component == c));

    for (auto& f : rs)
      r.push_back (f);

    t.commit ();

    launcher::log::trace_l3 (categories::cache {},
                             "query returned {} files",
                             r.size ());
    return r;
  }

  vector<cached_file> cache_database::
  files (const string& v) const
  {
    launcher::log::trace_l2 (categories::cache {},
                             "querying all cached files for version {}",
                             v);

    using query = odb::query<cached_file>;
    vector<cached_file> r;

    odb::transaction t (db_->begin ());
    odb::result<cached_file> rs (
      db_->query<cached_file> (query::version == v));

    for (auto& f : rs)
      r.push_back (f);

    t.commit ();

    launcher::log::trace_l3 (categories::cache {},
                             "query returned {} files",
                             r.size ());
    return r;
  }

  size_t cache_database::
  count () const
  {
    odb::transaction t (db_->begin ());
    odb::result<cached_file> rs (db_->query<cached_file> ());

    size_t n (0);

    for (auto i (rs.begin ()); i != rs.end (); ++i)
      ++n;

    t.commit ();

    launcher::log::trace_l3 (categories::cache {},
                             "counted {} total cached files",
                             n);
    return n;
  }

  size_t cache_database::
  count (component_type c) const
  {
    using query = odb::query<cached_file>;

    odb::transaction t (db_->begin ());
    odb::result<cached_file> rs (
      db_->query<cached_file> (query::component == c));

    size_t n (0);

    for (auto i (rs.begin ()); i != rs.end (); ++i)
      ++n;

    t.commit ();

    launcher::log::trace_l3 (categories::cache {},
                             "counted {} cached files for component {}",
                             n,
                             static_cast<int> (c));
    return n;
  }

  optional<component_version> cache_database::
  version (component_type c) const
  {
    launcher::log::trace_l3 (categories::cache {},
                             "querying version for component {}",
                             static_cast<int> (c));

    odb::transaction t (db_->begin ());
    shared_ptr<component_version> v (db_->find<component_version> (c));
    t.commit ();

    if (v)
      launcher::log::trace_l3 (categories::cache {},
                               "component version found: {}",
                               v->tag ());
    else
      launcher::log::trace_l3 (categories::cache {},
                               "component version not found");

    return v ? optional<component_version> (*v) : nullopt;
  }

  void cache_database::
  version (component_type c, const string& v)
  {
    launcher::log::info (categories::cache {},
                         "updating version for component {} to {}",
                         static_cast<int> (c),
                         v);

    // Update the existing version. Insert a new one if this is the first
    // time we see this component.
    //
    odb::transaction t (db_->begin ());

    shared_ptr<component_version> e (db_->find<component_version> (c));

    if (e)
    {
      e->set_tag (v);
      e->set_installed_at (current_timestamp ());
      db_->update (*e);
    }
    else
    {
      component_version cv (c, v, current_timestamp ());
      db_->persist (cv);
    }

    t.commit ();
  }

  void cache_database::
  erase_version (component_type c)
  {
    launcher::log::info (categories::cache {},
                         "erasing version for component {}",
                         static_cast<int> (c));

    odb::transaction t (db_->begin ());
    db_->erase<component_version> (c);
    t.commit ();
  }

  vector<component_version> cache_database::
  versions () const
  {
    launcher::log::trace_l2 (categories::cache {},
                             "querying all component versions");

    vector<component_version> r;

    odb::transaction t (db_->begin ());
    odb::result<component_version> rs (db_->query<component_version> ());

    for (auto& v : rs)
      r.push_back (v);

    t.commit ();

    launcher::log::trace_l3 (categories::cache {},
                             "query returned {} component versions",
                             r.size ());
    return r;
  }

  optional<user_setting> cache_database::
  setting (const string& k) const
  {
    launcher::log::trace_l3 (categories::cache {},
                             "querying user setting: {}",
                             k);

    odb::transaction t (db_->begin ());
    shared_ptr<user_setting> s (db_->find<user_setting> (k));
    t.commit ();

    if (s)
      launcher::log::trace_l3 (categories::cache {},
                               "user setting found: {} = {}",
                               k,
                               s->val ());
    else
      launcher::log::trace_l3 (categories::cache {},
                               "user setting not found: {}",
                               k);

    return s ? optional<user_setting> (*s) : nullopt;
  }

  string cache_database::
  setting_value (const string& k) const
  {
    launcher::log::trace_l3 (categories::cache {},
                             "querying user setting value directly: {}",
                             k);

    auto s (setting (k));
    return s ? s->val () : string ();
  }

  void cache_database::
  setting (const string& k, const string& v)
  {
    launcher::log::info (categories::cache {},
                         "writing user setting: {} = {}",
                         k,
                         v);

    // Overwrite or create the setting depending on whether it exists.
    //
    odb::transaction t (db_->begin ());

    shared_ptr<user_setting> e (db_->find<user_setting> (k));

    if (e)
    {
      launcher::log::trace_l3 (categories::cache {},
                               "updating existing user setting: {}",
                               k);
      e->val (v);
      db_->update (*e);
    }
    else
    {
      launcher::log::trace_l3 (categories::cache {},
                               "persisting new user setting: {}",
                               k);
      user_setting s (k, v);
      db_->persist (s);
    }

    t.commit ();
  }

  void cache_database::
  erase_setting (const string& k)
  {
    launcher::log::info (categories::cache {}, "erasing user setting: {}", k);

    odb::transaction t (db_->begin ());
    db_->erase<user_setting> (k);
    t.commit ();
  }

  void cache_database::
  vacuum ()
  {
    launcher::log::info (categories::cache {}, "vacuuming cache database");

    // Note that VACUUM cannot run inside a transaction.
    //
    db_->execute ("VACUUM");
  }

  bool cache_database::
  check () const
  {
    launcher::log::trace_l1 (categories::cache {},
                             "running PRAGMA integrity_check");

    odb::transaction t (db_->begin ());
    bool ok (true);

    odb::sqlite::connection& c (
      static_cast<odb::sqlite::connection&> (t.connection ()));

    sqlite3_stmt* s (nullptr);
    const char* q ("PRAGMA integrity_check");

    if (sqlite3_prepare_v2 (c.handle (), q, -1, &s, nullptr) == SQLITE_OK)
    {
      if (sqlite3_step (s) == SQLITE_ROW)
      {
        const char* r (
          reinterpret_cast<const char*> (sqlite3_column_text (s, 0)));

        // Run the SQLite integrity check. It returns multiple rows but if
        // everything is fine, the first row should just be 'ok'.
        //
        if (r == nullptr || string (r) != "ok")
        {
          launcher::log::error (categories::cache {},
                                "integrity check failed, result: {}",
                                r ? r : "null");
          ok = false;
        }
        else
          launcher::log::debug (categories::cache {}, "integrity check passed");
      }

      sqlite3_finalize (s);
    }
    else
    {
      launcher::log::error (categories::cache {},
                            "sqlite3_prepare_v2 failed for integrity_check");
      ok = false;
    }

    t.commit ();
    return ok;
  }

  void cache_database::
  clear ()
  {
    launcher::log::warning (categories::cache {},
                            "clearing cache database (all files and versions)");

    // Drop all cached entries and versions to reset the state.
    //
    odb::transaction t (db_->begin ());

    db_->erase_query<cached_file> ();
    db_->erase_query<component_version> ();

    t.commit ();
  }
}
