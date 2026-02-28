#include <algorithm>
#include <atomic>
#include <exception>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

#include <launcher/launcher-manifest.hxx>

namespace launcher
{
  namespace asio = boost::asio;

  namespace
  {
    inline bool
    exists_quiet (const fs::path& p)
    {
      std::error_code ec;
      return fs::exists (p, ec);
    }

    inline std::uint64_t
    size_quiet (const fs::path& p)
    {
      std::error_code ec;
      auto s (fs::file_size (p, ec));
      return ec ? 0 : s;
    }
  }

  template <typename T>
  basic_reconciler<T>::
  basic_reconciler (db_type& db, const fs::path& r)
    : db_ (db),
      root_ (r),
      strat_ (traits::def_strat)
  {
    launcher::log::trace_l2 (categories::cache{}, "initialized basic_reconciler with root: {}", root_.string ());
  }

  template <typename T>
  void basic_reconciler<T>::
  mode (strategy s)
  {
    strat_ = s;
    launcher::log::trace_l2 (categories::cache{}, "reconciler strategy changed to {}", static_cast<int> (s));
  }

  template <typename T>
  strategy basic_reconciler<T>::
  mode () const noexcept
  {
    return strat_;
  }

  template <typename T>
  void basic_reconciler<T>::
  progress (progress_cb cb)
  {
    cb_ = std::move (cb);
  }

  template <typename T>
  bool basic_reconciler<T>::
  outdated (component_type c, const str_type& tag) const
  {
    // The database acts as our intent registry. If we have no record for this
    // component, it implies either a fresh install or a corrupted DB state
    // where the user wiped the index but left the files. In either case, we
    // must treat it as outdated to force a full planning pass.
    //
    // However, if the tags match, we blindly trust the installation. We make
    // this compromise to avoid the significant latency of a full filesystem
    // scan on every application startup. If the DB says "we have version 1.2"
    // and the manifest wants "1.2", we assume the files on disk are intact
    // (famous last words).
    //
    // Note that the user can override this by explicitly requesting a audit
    // operation, which bypasses this check.
    //
    auto v (db_.version (c));
    bool is_outdated (!v || v->tag () != tag);

    launcher::log::debug (categories::cache{}, "component {} outdated check vs '{}': {}", static_cast<int> (c), tag, is_outdated);
    return is_outdated;
  }

  template <typename T>
  std::optional<typename basic_reconciler<T>::str_type>
  basic_reconciler<T>::
  version (component_type c) const
  {
    auto v (db_.version (c));
    return v ? std::make_optional (v->tag ()) : std::nullopt;
  }

  template <typename T>
  file_state basic_reconciler<T>::
  stat (const fs::path& p) const
  {
    // We attempt to minimize I/O by only stat'ing the actual filesystem if we
    // have a corresponding database record.
    //
    // If the DB doesn't know about this path, we return 'unknown'. This
    // covers two scenarios:
    //
    // 1. It is a new file introduced in a new manifest version we are
    //    currently planning for.
    //
    // 2. It is an orphan file left over from a previous version or a failed
    //    install.
    //
    str_type k (key (p));
    auto f (db_.find (k));

    if (!f)
    {
      launcher::log::trace_l3 (categories::cache{}, "stat missing db record for {}, returning unknown", p.string ());
      return file_state::unknown;
    }

    return stat (p, *f);
  }

  template <typename T>
  file_state basic_reconciler<T>::
  stat (const fs::path& p, const cached_file& f) const
  {
    // The DB record acts as a claim: "we successfully downloaded/extracted
    // this file at time T with size S". The reality, however, is on disk.
    //
    // We have to call fs::exists() here. While standard filesystem calls can
    // be slow on Windows, we cannot skip this: a user might have manually
    // deleted the file while leaving the DB intact.
    //
    // Note that we delegate the more expensive validity checks (comparing
    // hash or mtime) to the match() function below.
    //
    if (!exists_quiet (p))
    {
      launcher::log::trace_l3 (categories::cache{}, "stat: file missing from disk: {}", p.string ());
      return file_state::missing;
    }

    if (!match (p, f))
    {
      launcher::log::trace_l3 (categories::cache{}, "stat: file metadata stale: {}", p.string ());
      return file_state::stale;
    }

    launcher::log::trace_l3 (categories::cache{}, "stat: file valid: {}", p.string ());
    return file_state::valid;
  }

  template <typename T>
  std::vector<std::pair<cached_file, file_state>>
  basic_reconciler<T>::
  audit (component_type c) const
  {
    launcher::log::info (categories::cache{}, "starting heavy audit for component {}", static_cast<int> (c));

    // Note that this is the "slow path". That is, We retrieve everything we
    // *think* we have from the database and rigorously verify it against the
    // physical disk.
    //
    // We don't worry about performance here as much as correctness. This
    // function is rarely called during the hot path of a standard launch.
    //
    auto fs (db_.files (c));
    std::vector<std::pair<cached_file, file_state>> r;
    r.reserve (fs.size ());

    for (const auto& f : fs)
    {
      fs::path p (f.path ());
      r.emplace_back (f, stat (p, f));
    }

    launcher::log::debug (categories::cache{}, "audit complete for component {}, checked {} files", static_cast<int> (c), r.size ());
    return r;
  }

  // Data structure to capture the state of a parallel verification task.
  //
  // We cannot easily use a lambda capture for the output variables because
  // we need a stable memory location that outlives the temporary closure
  // passed to the thread pool, but is aggregated easily at the end.
  //
  struct hash_task
  {
    fs::path       p;
    std::string    exp; // The expected hash from the manifest.
    std::string    key; // The canonical DB key.
    component_type c;

    // Results (filled by the worker thread).
    //
    bool          match;
    std::int64_t  mtime;
    std::uint64_t size;
    std::string   error; // If non-empty, implies an exception occurred.
  };

  inline void
  run_hashes (std::vector<hash_task>& ts,
              std::function<void (const std::string&,
                                  std::size_t,
                                  std::size_t)> cb)
  {
    if (ts.empty ()) return;

    // We need to be conservative with our concurrency choices here.
    //
    // First, hardware_concurrency() can lie and return 0. In that case, we
    // fallback to 4. Now you may ask "Why 4"? Well, 4 is small enough not to
    // choke a dual-core laptop but large enough to get some overlap between
    // I/O bound tasks (reading the file) and CPU bound tasks (computing
    // BLAKE3).
    //
    // Note also that even if we have 64 cores, we might be I/O throttled by
    // the disk bandwidth, so throwing 64 threads at it might just cause
    // thrashing, but let's trust the standard library hint anyways.
    //
    unsigned int n (std::thread::hardware_concurrency ());
    if (n == 0) n = 4;

    launcher::log::trace_l2 (categories::cache{}, "spinning up thread pool with {} workers for {} hash tasks", n, ts.size ());

    asio::thread_pool pool (n);
    std::atomic<std::size_t> d (0); // Completed count.
    std::size_t tot (ts.size ());

    // We use a latch to fence execution. We cannot proceed to the
    // reconcile decision phase until *all* hashes are computed. The
    // alternative would be a complex chain of futures, but a latch is
    // simpler for this "fork-join" pattern.
    //
    std::latch l (static_cast<std::ptrdiff_t> (tot));

    for (auto& t : ts)
    {
      asio::post (pool,
                  [&t, &d, tot, &l, &cb] ()
      {
        // We must wrap the unit of work in a try-catch block. If a thread
        // throws (e.g., bad allocation), it could terminate the pool or the
        // program. We want to capture that failure and mark the file as
        // "mismatched" so the reconciler simply downloads it again.
        //
        try
        {
          // Check existence again inside the thread to avoid TOCTOU races,
          // though strict atomicity isn't required here.
          //
          if (exists_quiet (t.p) && !t.exp.empty ())
          {
            std::string a (compute_blake3 (t.p));
            t.match = (a == t.exp);

            // If the hash matches, we grab the stat data (mtime, size)
            // immediately. The OS likely has the inode in cache right now. If
            // we waited until the main thread resumed, the cache might be
            // cold again. "Do you wanna build a snowman?"
            //
            if (t.match)
            {
              t.mtime = get_file_mtime (t.p);
              t.size = size_quiet (t.p);
            }
          }
          else
          {
            t.match = false;
          }
        }
        catch (const std::exception& e)
        {
          // We don't abort on error, just assume the file is broken.
          //
          launcher::log::warning (categories::cache{}, "exception during parallel hash for {}: {}", t.p.string (), e.what ());
          t.match = false;
          t.error = e.what ();
        }
        catch (...)
        {
          launcher::log::warning (categories::cache{}, "unknown exception during parallel hash for {}", t.p.string ());
          t.match = false;
          t.error = "unknown exception during hashing";
        }

        std::size_t c (++d);
        if (cb)
          cb ("Verifying", c, tot);

        l.count_down ();
      });
    }

    // Wait for the pool to drain.
    l.wait ();
    pool.join ();
    launcher::log::trace_l2 (categories::cache{}, "all hash tasks completed");
  }

  template <typename T>
  std::vector<reconcile_item>
  basic_reconciler<T>::
  plan (const manifest& m, component_type c, const str_type& v)
  {
    launcher::log::trace_l1 (categories::cache{}, "generating reconcile plan for component {} (target version {})", static_cast<int> (c), v);
    std::vector<reconcile_item> r;

    // The order of planning operations is significant.
    //
    // We process archives first because they often act as containers for
    // multiple files. If the manifest defines an "exploded" archive (one that
    // unzips into individual files), the validity of that archive effectively
    // determines the validity of the files within it.
    //
    // So what we want here is to identify which files are covered by archive
    // rules to prevent double-booking them in the individual file check pass
    // later.
    //
    auto as (plan_archives (m.archives, c, v));
    r.insert (r.end (),
              std::make_move_iterator (as.begin ()),
              std::make_move_iterator (as.end ()));

    auto fs (plan_files (m.files, c, v));
    r.insert (r.end (),
              std::make_move_iterator (fs.begin ()),
              std::make_move_iterator (fs.end ()));

    launcher::log::info (categories::cache{}, "reconcile plan generated with {} items", r.size ());
    return r;
  }

  template <typename T>
  std::vector<reconcile_item>
  basic_reconciler<T>::
  plan_archives (const std::vector<manifest_archive>& as,
                 component_type c,
                 const str_type& v)
  {
    launcher::log::trace_l2 (categories::cache{}, "planning {} archives", as.size ());
    std::vector<reconcile_item> r;

    // Reconciliation of archives is non-trivial compared to simple files. We
    // have to handle two distinct cases:
    //
    // 1. "Exploded" Archives: The manifest lists inner files that this
    //    archive provides. In this case, we check the validity of those inner
    //    files on disk.
    //
    // 2. "Blob" Archives: The manifest lists no inner files. We treat the
    //    archive itself as the artifact.
    //
    // Note also that we don't want to hash these one by one serially, so
    // instead we build a queue of verification tasks and run them in parallel
    // via run_hashes().
    //
    struct link_info
    {
      std::size_t i; // Index into 'as' vector.
      fs::path    p;
      std::string h; // Expected hash.
      std::string k; // DB key.
    };

    struct file_info
    {
      std::size_t i;
      fs::path    p;
      std::string h;
      std::string k;
    };

    std::vector<link_info> ls;
    std::vector<file_info> fs;

    struct state
    {
      bool skip;      // If true, the item is valid and needs no action.
      bool has_links; // True if this is an exploded archive.
      bool links_ok;  // Accumulator for the validity of inner files.
      bool dl;        // Final decision: do we download?
    };

    std::vector<state> ss (as.size ());

    // First, we must scan the manifest and DB to determine what is physically
    // missing or metadata-stale.
    //
    for (std::size_t i (0); i < as.size (); ++i)
    {
      const auto& a (as[i]);
      auto& s (ss[i]);

      s.skip = false;
      s.has_links = !a.files.empty ();
      s.links_ok = true;
      s.dl = false;

      // Corner case: If there is no URL, we cannot repair it.
      //
      if (a.url.empty ())
      {
        launcher::log::trace_l3 (categories::cache{}, "skipping archive {} (no URL provided)", a.name);
        s.skip = true;
        continue;
      }

      if (s.has_links)
      {
        // For exploded archives, we iterate over the files it promises to
        // deliver. If *any single one* is missing or corrupted, we invalidate
        // the entire archive.
        //
        // @@: native partial fetch/repair (e.g., cloudzip) could make this
        //     finer-grained, but in practice these archives mostly come from
        //     GitHub releases, so the added complexity is not justified here
        //
        for (const auto& f : a.files)
        {
          fs::path p (root_ / f.path);
          str_type k (key (p));
          auto cached (db_.find (k));

          // If we have a cache hit, we check if it matches our criteria
          // (timestamp/size). If not cached but present on disk, we queue it
          // for hashing to see if we can "adopt" it without downloading.
          //
          if (cached)
          {
            if (stat (p, *cached) != file_state::valid)
            {
              launcher::log::trace_l3 (categories::cache{}, "exploded archive {} inner file stale: {}", a.name, p.string ());
              s.links_ok = false;
            }
          }
          else if (exists_quiet (p) && !f.hash.value.empty ())
          {
            ls.push_back ({i, p, f.hash.value, k});
          }
          else
          {
            // Missing from both DB and disk.
            //
            launcher::log::trace_l3 (categories::cache{}, "exploded archive {} inner file missing: {}", a.name, p.string ());
            s.links_ok = false;
          }
        }
      }
      else
      {
        // Standalone archive (blob).
        //
        fs::path p (path (a));
        str_type k (key (p));
        auto cached (db_.find (k));

        if (cached)
        {
          // If the version changed, we force a redownload. We don't try to
          // binary-diff archives.
          //
          if (cached->version () != v || stat (p, *cached) != file_state::valid)
          {
            launcher::log::trace_l3 (categories::cache{}, "blob archive {} version mismatch or stale", a.name);
            s.dl = true;
          }

          s.skip = !s.dl;
        }
        else if (exists_quiet (p) && !a.hash.value.empty ())
        {
          // It exists on disk but the DB doesn't know about it. This happens
          // if the DB was deleted. Queue for hashing.
          //
          fs.push_back ({i, p, a.hash.value, k});
        }
        else
        {
          launcher::log::trace_l3 (categories::cache{}, "blob archive {} missing from disk/db", a.name);
          s.dl = true;
        }
      }
    }

    // Execute the hash verification queue, but only proceed if we actually
    // have work.
    //
    if (!ls.empty () || !fs.empty ())
    {
      std::vector<hash_task> ts;
      ts.reserve (ls.size () + fs.size ());

      for (const auto& l : ls)
        ts.push_back ({l.p, l.h, l.k, c, false, 0, 0, ""});

      for (const auto& f : fs)
        ts.push_back ({f.p, f.h, f.k, c, false, 0, 0, ""});

      run_hashes (ts, cb_);

      // Apply results from the worker threads to our state.
      //
      auto it (ts.begin ());

      for (const auto& l : ls)
      {
        const auto& t (*it++);

        // If the hash matched, we adopt the file into the DB. If not, the
        // archive that owns this file is marked as broken.
        //
        if (t.match)
        {
          launcher::log::trace_l3 (categories::cache{}, "adopting existing file into db: {}", l.k);
          db_.store (cached_file (l.k, t.mtime, v, c, t.size, l.h));
        }
        else
        {
          launcher::log::trace_l3 (categories::cache{}, "hash mismatch for {}, invalidating parent archive", l.p.string ());
          ss[l.i].links_ok = false;
        }
      }

      for (const auto& f : fs)
      {
        const auto& t (*it++);
        auto& s (ss[f.i]);

        if (t.match)
        {
          launcher::log::trace_l3 (categories::cache{}, "adopting existing blob archive into db: {}", f.k);
          db_.store (cached_file (f.k, t.mtime, v, c, t.size, f.h));
          s.skip = true;
        }
        else
        {
          s.dl = true;
        }
      }
    }

    // Translate the internal state into public reconcile items.
    //
    for (std::size_t i (0); i < as.size (); ++i)
    {
      const auto& a (as[i]);
      const auto& s (ss[i]);

      if (s.skip) continue;

      bool dl (s.dl);

      // As mentioned above, if even one link in the exploded archive is bad,
      // we must download the whole archive again.
      //
      if (s.has_links && !s.links_ok)
        dl = true;

      if (dl)
      {
        reconcile_item ri;
        ri.action = reconcile_action::download;
        ri.path = s.has_links ? (root_ / a.name).string () : path (a).string ();
        ri.url = a.url;
        ri.expected_hash = a.hash.value;
        ri.expected_size = a.size;
        ri.component = c;
        ri.version = v;

        launcher::log::trace_l3 (categories::cache{}, "archive item added to reconcile plan: {}", ri.path);
        r.push_back (std::move (ri));
      }
    }

    return r;
  }

  template <typename T>
  std::vector<reconcile_item>
  basic_reconciler<T>::
  plan_files (const std::vector<manifest_file>& fs,
              component_type c,
              const str_type& v)
  {
    launcher::log::trace_l2 (categories::cache{}, "planning {} standalone files", fs.size ());
    std::vector<reconcile_item> r;
    std::size_t i (0);

    // Unlike archives, we process standalone files serially.
    //
    // The number of standalone files is usually low compared to the bulk data
    // inside archives, so the overhead of spinning up threads for small files
    // often outweighs the gain.
    //
    for (const auto& f : fs)
    {
      report ("Checking " + f.path, ++i, fs.size ());

      // If a file claims to belong to an archive, we skip it here. The
      // plan_archives() pass has already adjudicated its fate.
      //
      if (f.archive_name) continue;

      // Skip manifest files. These describe the update, not files to install.
      //
      if (f.path.ends_with ("update.json")) continue;

      fs::path p (path (f));
      str_type k (key (p));
      auto cached (db_.find (k));
      bool dl (false);

      if (!cached)
      {
        // Missing from DB.
        //
        if (!exists_quiet (p))
        {
          launcher::log::trace_l3 (categories::cache{}, "file {} missing from disk and db", f.path);
          dl = true;
        }
        else if (!f.hash.value.empty ())
        {
          // Exists on disk, but unknown to DB.
          //
          // We hash it to see if we can adopt it (e.g. if the user copied the
          // files from another machine but didn't copy the DB).
          //
          report ("Hashing " + f.path, i, fs.size ());

          try
          {
            if (verify_blake3 (p, f.hash.value))
            {
              launcher::log::trace_l3 (categories::cache{}, "file {} matches expected hash, adopting to db", f.path);
              // Match found. Store in DB and skip download.
              //
              db_.store (cached_file (k,
                                      get_file_mtime (p),
                                      v,
                                      c,
                                      fs::file_size (p),
                                      f.hash.value));
            }
            else
            {
              launcher::log::trace_l3 (categories::cache{}, "file {} hash mismatch on adoption attempt", f.path);
              dl = true;
            }
          }
          catch (...)
          {
            // If we can't read/verify (e.g., locked file), we force a
            // redownload.
            //
            launcher::log::warning (categories::cache{}, "exception during hash verification for {}", f.path);
            dl = true;
          }
        }
        else
        {
          dl = true;
        }
      }
      else
      {
        // Known to DB. Check validity.
        //
        if (cached->version () != v)
        {
          launcher::log::trace_l3 (categories::cache{}, "file {} version mismatch (cache: {}, target: {})", f.path, cached->version (), v);
          dl = true;
        }
        else if (stat (p, *cached) != file_state::valid)
        {
          launcher::log::trace_l3 (categories::cache{}, "file {} state is invalid/stale", f.path);
          dl = true;
        }
      }

      if (dl && f.asset_name)
      {
        reconcile_item ri;
        ri.action = reconcile_action::download;
        ri.path = p.string ();
        ri.expected_hash = f.hash.value;
        ri.expected_size = f.size;
        ri.component = c;
        ri.version = v;
        r.push_back (std::move (ri));

        launcher::log::trace_l3 (categories::cache{}, "file item added to reconcile plan: {}", ri.path);
      }
    }

    return r;
  }

  template <typename T>
  reconcile_summary basic_reconciler<T>::
  summarize (const std::vector<reconcile_item>& items) const
  {
    launcher::log::trace_l3 (categories::cache{}, "generating summary for {} items", items.size ());
    reconcile_summary s;

    // Simple aggregation for reporting purposes.
    //
    for (const auto& i : items)
    {
      switch (i.action)
      {
        case reconcile_action::download:
          s.downloads_required++, s.bytes_to_download += i.expected_size;
          break;

        case reconcile_action::verify: s.files_stale++;   break;
        case reconcile_action::remove: s.files_unknown++; break;
        case reconcile_action::none:   s.files_valid++;   break;
      }
    }

    return s;
  }

  template <typename T>
  void basic_reconciler<T>::
  track (const fs::path& p,
         component_type c,
         const str_type& v,
         const str_type& h)
  {
    // We only track files that successfully made it to disk. If the
    // extraction failed, the file won't exist, and we shouldn't record a lie
    // in the DB.
    //
    if (!exists_quiet (p))
    {
      launcher::log::warning (categories::cache{}, "attempted to track non-existent file: {}", p.string ());
      return;
    }

    try
    {
      launcher::log::trace_l3 (categories::cache{}, "tracking file: {}", p.string ());
      db_.store (cached_file (key (p),
                              get_file_mtime (p),
                              v,
                              c,
                              fs::file_size (p),
                              h));
    }
    catch (const std::exception& e)
    {
      launcher::log::warning (categories::cache{}, "exception tracking file {}: {}", p.string (), e.what ());
      // If the DB write fails (disk full, lock), failing the whole launcher
      // process is user-hostile, so instead let's allow incomplete cache as
      // it will self-heal on next run via the "exists but unknown" path
      //
    }
    catch (...)
    {
      launcher::log::warning (categories::cache{}, "unknown exception tracking file {}", p.string ());
    }
  }

  template <typename T>
  void basic_reconciler<T>::
  track (const std::vector<fs::path>& ps,
         component_type c,
         const str_type& v)
  {
    launcher::log::trace_l2 (categories::cache{}, "batch tracking {} files", ps.size ());
    // Batch for large archive where updating sqlite row-by-row would be too
    // slow due to transaction overhead.
    //
    std::vector<cached_file> es;
    es.reserve (ps.size ());

    for (const auto& p : ps)
    {
      if (!exists_quiet (p)) continue;
      try
      {
        es.emplace_back (key (p),
                         get_file_mtime (p),
                         v,
                         c,
                         fs::file_size (p),
                         "");
      }
      catch (...)
      {
        // Skip files we can't stat.
      }
    }

    if (!es.empty ())
    {
      launcher::log::trace_l3 (categories::cache{}, "storing {} stat'd files to db", es.size ());
      db_.store (es);
    }
  }

  template <typename T>
  void basic_reconciler<T>::
  stamp (component_type c, const str_type& tag)
  {
    launcher::log::info (categories::cache{}, "stamping component {} with version tag {}", static_cast<int> (c), tag);
    db_.version (c, tag);
  }

  template <typename T>
  void basic_reconciler<T>::
  forget (const fs::path& p)
  {
    launcher::log::trace_l3 (categories::cache{}, "forgetting file from db: {}", p.string ());
    db_.erase (key (p));
  }

  template <typename T>
  std::vector<typename basic_reconciler<T>::str_type>
  basic_reconciler<T>::
  clean (const manifest& m, component_type c)
  {
    launcher::log::info (categories::cache{}, "cleaning orphaned files for component {}", static_cast<int> (c));

    // Our goal here is to identify "orphans": entries that we currently track
    // in the database but which are no longer referenced by the version of
    // the manifest we are processing. This usually happens during an upgrade
    // (where files are renamed or removed) or if a previous installation was
    // partial.
    //
    // The logic is effectively a set difference: Orphans = DB - Manifest.
    //
    // Note that we need to be careful with performance here. While
    // constructing a std::set for the manifest entries and iterating the DB
    // would be the textbook approach, we opt for a sorted std::vector. That
    // is, the overhead of allocating thousands of node objects for a std::set
    // (one per file) often outweighs the cost of a single contiguous
    // allocation and a sort, especially given that we only do read-only
    // lookups afterwards.
    //
    std::vector<str_type> orphans;
    std::vector<str_type> expect;

    // Build the "expected" set from the manifest.
    //
    // We include all archive packages and all standalone files.
    //
    // Note that we explicitly exclude files that are technically "inside" an
    // archive (those where f.archive_name is set). We do this because the
    // archive itself is the atomic unit of lifecycle management for the
    // reconciler. If the archive entry exists in the manifest, we assume its
    // contents are managed by the archive extraction logic. If we were to
    // track inner files individually here, we might accidentally try to clean
    // up a file that is currently being extracted.
    //
    for (const auto& a : m.archives)
      expect.push_back (key (path (a)));

    for (const auto& f : m.files)
      if (!f.archive_name)
        expect.push_back (key (path (f)));

    // Sort the expectations so we can use binary search for O(log N)
    // lookups.
    //
    std::sort (expect.begin (), expect.end ());

    // Retrieve the full state of what we *think* is installed.
    //
    auto cached (db_.files (c));

    for (const auto& f : cached)
    {
      // If the file is in the DB but not in our expected list, it is an
      // orphan.
      //
      // Note that this implies that if a standalone file is moved into an
      // archive in a newer version, the old standalone DB entry will be
      // flagged as an orphan and removed here. This is correct: the file on
      // disk will arguably be overwritten or adopted by the archive
      // extraction later.
      //
      if (!std::binary_search (expect.begin (), expect.end (), f.path ()))
        orphans.push_back (f.path ());
    }

    launcher::log::debug (categories::cache{}, "found {} orphaned database entries", orphans.size ());

    // Commit the cleanup.
    //
    // We only remove the records from the DB here. We do not physically
    // delete the files at this stage. That is, physical removal is a
    // separate, more dangerous operation that usually requires specific
    // permissions or user consent (e.g., during the "uninstall" phase). Leave
    // it to the garbage collector to pick it up later if needed.
    //
    if (traits::auto_prune && !orphans.empty ())
    {
      launcher::log::trace_l2 (categories::cache{}, "auto-pruning orphans from db");
      db_.erase (orphans);
    }

    return orphans;
  }

  template <typename T>
  fs::path basic_reconciler<T>::
  path (const manifest_file& f) const
  {
    return manifest_coordinator::resolve_path (f, root_);
  }

  template <typename T>
  fs::path basic_reconciler<T>::
  path (const manifest_archive& a) const
  {
    return manifest_coordinator::resolve_path (a, root_);
  }

  template <typename T>
  typename basic_reconciler<T>::str_type
  basic_reconciler<T>::
  key (const fs::path& p) const
  {
    // The DB key must be a canonical string to avoid duplication.
    //
    // If we have "foo/bar" and "foo/./bar", they point to the same file. We
    // use weakly_canonical because the file might not exist yet (during plan
    // generation) but we still need a stable key for the DB.
    //
    std::error_code ec;
    fs::path can (fs::weakly_canonical (p, ec));

    // If canonicalization fails (e.g., permission error on parent directory),
    // we fallback to the raw string path.
    //
    str_type res (ec ? p.string () : can.string ());
    launcher::log::trace_l3 (categories::cache{}, "generated key for path {}: {}", p.string (), res);
    return res;
  }

  template <typename T>
  void basic_reconciler<T>::
  report (const str_type& msg, std::size_t cur, std::size_t tot)
  {
    if (cb_) cb_ (msg, cur, tot);
  }

  template <typename T>
  bool basic_reconciler<T>::
  match (const fs::path& p, const cached_file& f) const
  {
    try
    {
      launcher::log::trace_l3 (categories::cache{}, "matching file {} (expected mtime: {}, size: {})", p.string (), f.mtime (), f.size ());

      // Mtime is our primary optimization. If the timestamp hasn't changed,
      // we assume the content hasn't changed.
      //
      if (get_file_mtime (p) != f.mtime ())
      {
        launcher::log::trace_l3 (categories::cache{}, "mtime mismatch for {}", p.string ());
        return false;
      }

      // In 'paranoid' (mixed) or 'repair' (hash) modes, we dig deeper.
      //
      // Currently, we only check file size here as a cheap secondary check.
      // True hash verification happens earlier in the planning phase if
      // required.
      //
      if (strat_ == strategy::mixed || strat_ == strategy::hash)
      {
        if (fs::file_size (p) != f.size ())
        {
          launcher::log::trace_l3 (categories::cache{}, "size mismatch for {}", p.string ());
          return false;
        }
      }

      launcher::log::trace_l3 (categories::cache{}, "match found for {}", p.string ());
      return true;
    }
    catch (const std::exception& e)
    {
      launcher::log::warning (categories::cache{}, "exception during match for {}: {}", p.string (), e.what ());

      // If we cannot access metadata (permissions, file locked), we must
      // assume the file is invalid to trigger a safe recovery path.
      //
      return false;
    }
    catch (...)
    {
      launcher::log::warning (categories::cache{}, "unknown exception during match for {}", p.string ());
      return false;
    }
  }
}
