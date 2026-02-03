#include <odb/query.hxx>
#include <odb/result.hxx>

// Include ODB-generated headers.
//
#include <launcher/cache/cache-types-odb.hxx>

namespace launcher
{
  template <typename T>
  basic_cache_database<T>::
  basic_cache_database (const fs::path& d)
  {
    init (d);
  }

  template <typename T>
  basic_cache_database<T>::
  ~basic_cache_database ()
  {
  }

  template <typename T>
  void basic_cache_database<T>::
  init (const fs::path& d)
  {
    // We stick the cache database into the standard cache directory layout.
    //
    fs::path c (d / traits_type::dir_name);

    if (!fs::exists (c))
    {
      std::error_code ec;
      fs::create_directories (c, ec);

      if (ec)
        throw std::runtime_error (
          "failed to create cache directory: " + c.string () +
          ": " + ec.message ());
    }

    path_ = c / traits_type::db_name;

    // Check if the file exists before we open the DB so we know if
    // we need to bootstrap the schema.
    //
    bool create (!fs::exists (path_));

    // Open with create flag (default for sqlite::database).
    //
    db_ = std::make_unique<database_type> (
      path_.string (),
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

    // Tweak the engine before doing anything else.
    //
    pragmas ();

    if (create || traits_type::auto_create)
      schema ();
  }

  template <typename T>
  void basic_cache_database<T>::
  schema ()
  {
    // ODB's create_schema() will fail if the table already exists, but it
    // doesn't offer a clean "if_not_exists" check. So we have to manually
    // peek at sqlite_master to see if our tables are there.
    //
    bool exists (false);
    {
      odb::transaction t (db_->begin ());

      odb::sqlite::connection& c (
        static_cast<odb::sqlite::connection&> (t.connection ()));

      sqlite3_stmt* s (nullptr);
      const char* q (
        "SELECT name FROM sqlite_master WHERE type='table' AND name='cached_files'");

      if (sqlite3_prepare_v2 (c.handle (), q, -1, &s, nullptr) == SQLITE_OK)
      {
        if (sqlite3_step (s) == SQLITE_ROW)
          exists = true;
        sqlite3_finalize (s);
      }

      t.commit ();
    }

    // Since create_schema manages its own transaction, the previous check
    // must be fully committed.
    //
    if (!exists)
    {
      odb::transaction t (db_->begin ());
      odb::schema_catalog::create_schema (*db_);
      t.commit ();
    }
  }

  template <typename T>
  void basic_cache_database<T>::
  pragmas ()
  {
    // We need to drop down to the raw handle because ODB's execute() enforces
    // a transaction, but some pragmas (WAL, synchronous) require being
    // outside of one.
    //
    odb::connection_ptr c (db_->connection ());
    odb::sqlite::connection& sc (
      static_cast<odb::sqlite::connection&> (*c));
    sqlite3* h (sc.handle ());

    // WAL allows better concurrency if we ever have multiple processes,
    // though currently we lock the whole thing.
    //
    if (traits_type::wal)
      sqlite3_exec (h, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);

    // We prioritize performance. If someting corrupt, we just rebuild the
    // cache.
    //
    sqlite3_exec (h, "PRAGMA synchronous=OFF", nullptr, nullptr, nullptr);
    sqlite3_exec (h, "PRAGMA foreign_keys=OFF", nullptr, nullptr, nullptr);
    sqlite3_exec (h, "PRAGMA locking_mode=EXCLUSIVE", nullptr, nullptr, nullptr);
    sqlite3_exec (h, "PRAGMA temp_store=MEMORY", nullptr, nullptr, nullptr);
  }

  template <typename T>
  std::optional<cached_file> basic_cache_database<T>::
  find (const string_type& p) const
  {
    odb::transaction t (db_->begin ());
    std::shared_ptr<cached_file> f (db_->template find<cached_file> (p));
    t.commit ();

    return f ? std::optional<cached_file> (*f) : std::nullopt;
  }

  template <typename T>
  void basic_cache_database<T>::
  store (const cached_file& f)
  {
    odb::transaction t (db_->begin ());

    // We can't strictly use persist() because it might already exist, and we
    // don't want to rely on exceptions for control flow.
    //
    std::shared_ptr<cached_file> e (
      db_->template find<cached_file> (f.path ()));

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

    t.commit ();
  }

  template <typename T>
  void basic_cache_database<T>::
  store (const std::vector<cached_file>& fs)
  {
    if (fs.empty ())
      return;

    odb::transaction t (db_->begin ());

    // Batch update to minimize transaction overhead.
    //
    for (const auto& f : fs)
    {
      std::shared_ptr<cached_file> e (
        db_->template find<cached_file> (f.path ()));

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
  }

  template <typename T>
  void basic_cache_database<T>::
  erase (const string_type& p)
  {
    odb::transaction t (db_->begin ());
    db_->template erase<cached_file> (p);
    t.commit ();
  }

  template <typename T>
  void basic_cache_database<T>::
  erase (const std::vector<string_type>& ps)
  {
    if (ps.empty ())
      return;

    odb::transaction t (db_->begin ());

    for (const auto& p : ps)
      db_->template erase<cached_file> (p);

    t.commit ();
  }

  template <typename T>
  void basic_cache_database<T>::
  erase (component_type c)
  {
    using query = odb::query<cached_file>;

    odb::transaction t (db_->begin ());
    db_->template erase_query<cached_file> (query::component == c);
    t.commit ();
  }

  template <typename T>
  std::vector<cached_file> basic_cache_database<T>::
  files () const
  {
    std::vector<cached_file> r;

    odb::transaction t (db_->begin ());
    odb::result<cached_file> res (db_->template query<cached_file> ());

    for (auto& f: res)
      r.push_back (f);

    t.commit ();
    return r;
  }

  template <typename T>
  std::vector<cached_file> basic_cache_database<T>::
  files (component_type c) const
  {
    using query = odb::query<cached_file>;

    std::vector<cached_file> r;

    odb::transaction t (db_->begin ());
    odb::result<cached_file> res (
      db_->template query<cached_file> (query::component == c));

    for (auto& f: res)
      r.push_back (f);

    t.commit ();
    return r;
  }

  template <typename T>
  std::vector<cached_file> basic_cache_database<T>::
  files (const string_type& v) const
  {
    using query = odb::query<cached_file>;

    std::vector<cached_file> r;

    odb::transaction t (db_->begin ());
    odb::result<cached_file> res (
      db_->template query<cached_file> (query::version == v));

    for (auto& f: res)
      r.push_back (f);

    t.commit ();
    return r;
  }

  template <typename T>
  std::size_t basic_cache_database<T>::
  count () const
  {
    odb::transaction t (db_->begin ());
    odb::result<cached_file> r (db_->template query<cached_file> ());
    std::size_t n (0);

    for (auto i (r.begin ()); i != r.end (); ++i)
      ++n;

    t.commit ();
    return n;
  }

  template <typename T>
  std::size_t basic_cache_database<T>::
  count (component_type c) const
  {
    using query = odb::query<cached_file>;

    odb::transaction t (db_->begin ());
    odb::result<cached_file> r (
      db_->template query<cached_file> (query::component == c));

    std::size_t n (0);

    for (auto i (r.begin ()); i != r.end (); ++i)
      ++n;

    t.commit ();
    return n;
  }

  template <typename T>
  std::optional<component_version> basic_cache_database<T>::
  version (component_type c) const
  {
    odb::transaction t (db_->begin ());
    std::shared_ptr<component_version> v (
      db_->template find<component_version> (c));
    t.commit ();

    return v ? std::optional<component_version> (*v) : std::nullopt;
  }

  template <typename T>
  void basic_cache_database<T>::
  version (component_type c, const string_type& tag)
  {
    odb::transaction t (db_->begin ());

    std::shared_ptr<component_version> e (
      db_->template find<component_version> (c));

    if (e)
    {
      e->set_tag (tag);
      e->set_installed_at (current_timestamp ());
      db_->update (*e);
    }
    else
    {
      component_version v (c, tag, current_timestamp ());
      db_->persist (v);
    }

    t.commit ();
  }

  template <typename T>
  void basic_cache_database<T>::
  erase_version (component_type c)
  {
    odb::transaction t (db_->begin ());
    db_->template erase<component_version> (c);
    t.commit ();
  }

  template <typename T>
  std::vector<component_version> basic_cache_database<T>::
  versions () const
  {
    std::vector<component_version> r;

    odb::transaction t (db_->begin ());
    odb::result<component_version> res (
      db_->template query<component_version> ());

    for (auto& v: res)
      r.push_back (v);

    t.commit ();
    return r;
  }

  template <typename T>
  template <typename F>
  void basic_cache_database<T>::
  transact (F&& f)
  {
    odb::transaction t (db_->begin ());
    f ();
    t.commit ();
  }

  template <typename T>
  template <typename F>
  auto basic_cache_database<T>::
  transact_r (F&& f) -> decltype (f ())
  {
    odb::transaction t (db_->begin ());
    auto r (f ());
    t.commit ();
    return r;
  }

  template <typename T>
  void basic_cache_database<T>::
  vacuum ()
  {
    // VACUUM can't run inside a transaction.
    //
    db_->execute ("VACUUM");
  }

  template <typename T>
  bool basic_cache_database<T>::
  check () const
  {
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

        if (r == nullptr || std::string (r) != "ok")
          ok = false;
      }
      sqlite3_finalize (s);
    }

    t.commit ();
    return ok;
  }

  template <typename T>
  void basic_cache_database<T>::
  clear ()
  {
    odb::transaction t (db_->begin ());

    db_->template erase_query<cached_file> ();
    db_->template erase_query<component_version> ();

    t.commit ();
  }
}
