// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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

  // Launcher cache: the SQLite database with the records of the installed
  // files, the installed component versions, and the settings.
  //
  class cache_database
  {
  public:
    // Database file name and the directory, relative to the installation
    // root, that contains it.
    //
    static constexpr const char* db_name = "iw4x.db";
    static constexpr const char* dir_name = "cache";

    // Schema version. Note that we don't migrate the schema since the cache
    // is wiped whenever the launcher version changes (see
    // installation::reset_cache()).
    //
    static constexpr unsigned int schema_ver = 1;

    // Whether to create the schema of an existing database if its tables are
    // missing. The schema of a new database is always created.
    //
    static constexpr bool auto_create = true;

    // Whether to use the write-ahead log instead of the rollback journal.
    //
    static constexpr bool wal = true;

    // Open the cache database of the installation root, creating the cache
    // directory, the database, and its schema if they don't exist yet. Throw
    // std::runtime_error if the cache directory cannot be created and
    // odb::exception if the database cannot be opened.
    //
    explicit
    cache_database (const fs::path& root);

    cache_database (const cache_database&) = delete;
    cache_database& operator= (const cache_database&) = delete;

    ~cache_database ();

    // Return true if the database is open.
    //
    bool
    open () const noexcept;

    // Return the path of the database file.
    //
    const fs::path&
    path () const noexcept;

    // Return the underlying ODB database, for queries that the functions
    // below don't cover.
    //
    odb::sqlite::database&
    db () noexcept;

    const odb::sqlite::database&
    db () const noexcept;

  public:
    // File records. A file is identified by its normalized absolute path
    // (see reconciler::key()).
    //

    // Return the record of the file or nullopt if there is none.
    //
    std::optional<cached_file>
    find (const std::string& p) const;

    std::optional<cached_file>
    find (const fs::path& p) const;

    // Store the record, replacing the existing record of the same file, if
    // any. The second version stores all the records in a single
    // transaction.
    //
    void
    store (const cached_file& f);

    void
    store (const std::vector<cached_file>& fs);

    // Erase the record of the file, the records of the files, or the records
    // of all the files of the component, respectively.
    //
    void
    erase (const std::string& p);

    void
    erase (const std::vector<std::string>& ps);

    void
    erase (component_type c);

    // Return all the records, the records of the component, or the records
    // of the version, respectively.
    //
    std::vector<cached_file>
    files () const;

    std::vector<cached_file>
    files (component_type c) const;

    std::vector<cached_file>
    files (const std::string& v) const;

    // Return the number of all the records or of the records of the
    // component, respectively.
    //
    std::size_t
    count () const;

    std::size_t
    count (component_type c) const;

  public:
    // Component versions.
    //

    // Return the installed version of the component or nullopt if it is not
    // installed.
    //
    std::optional<component_version>
    version (component_type c) const;

    // Record the tag as the installed version of the component, installed
    // now.
    //
    void
    version (component_type c, const std::string& tag);

    void
    erase_version (component_type c);

    std::vector<component_version>
    versions () const;

  public:
    // Settings.
    //

    // Return the setting or nullopt if it is not set.
    //
    std::optional<user_setting>
    setting (const std::string& key) const;

    // Return the value of the setting or the empty string if it is not set.
    //
    std::string
    setting_value (const std::string& key) const;

    // Set the value of the setting, adding the setting if necessary.
    //
    void
    setting (const std::string& key, const std::string& value);

    void
    erase_setting (const std::string& key);

  public:
    // Transactions.
    //

    // Call the function in a transaction, committing the transaction if the
    // function returns and rolling it back if it throws. The second version
    // returns the result of the function.
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

  public:
    // Maintenance.
    //

    // Shrink the database file to its content. Note that SQLite doesn't
    // release the space of erased records on its own.
    //
    void
    vacuum ();

    // Return true if the SQLite integrity check passes.
    //
    bool
    check () const;

    // Erase all the file records and component versions. Note that the
    // settings are kept.
    //
    void
    clear ();

  private:
    void
    init (const fs::path& root);

    // Create the schema unless its tables already exist.
    //
    void
    schema ();

    // Configure SQLite for our use of the database (see the implementation
    // for details).
    //
    void
    pragmas ();

    fs::path path_;
    std::unique_ptr<odb::sqlite::database> db_;
  };
}
