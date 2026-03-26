#include <launcher/cache/cache-reconciler.hxx>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <exception>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <launcher/launcher-manifest.hxx>

using namespace std;

namespace launcher
{
  namespace asio = boost::asio;

  namespace
  {
    // Quietly check if a path exists. We don't want to throw here since
    // missing files are a normal occurrence during reconciliation.
    //
    bool
    exists_quiet (const fs::path& p)
    {
      error_code ec;
      return fs::exists (p, ec);
    }

    // Quietly get the file size. Return 0 on error, which is safe enough
    // for our cache heuristic checks.
    //
    uint64_t
    size_quiet (const fs::path& p)
    {
      error_code ec;
      auto s (fs::file_size (p, ec));
      return ec ? 0 : s;
    }
  }

  cache_database& reconciler::
  database () noexcept
  {
    // Expose the underlying database for mutable operations.
    //
    launcher::log::trace_l3 (categories::cache {},
                             "accessing mutable database reference");
    return db_;
  }

  const cache_database& reconciler::
  database () const noexcept
  {
    launcher::log::trace_l3 (categories::cache {},
                             "accessing const database reference");
    return db_;
  }

  const fs::path& reconciler::
  root () const noexcept
  {
    launcher::log::trace_l3 (categories::cache {},
                             "accessing root path: {}",
                             root_.string ());
    return root_;
  }

  reconciler::
  reconciler (cache_database& db, const fs::path& r)
    : db_ (db),
      root_ (r),
      strat_ (def_strat)
  {
    launcher::log::trace_l2 (categories::cache {},
                             "initialized reconciler with root: {}",
                             root_.string ());
  }

  void reconciler::
  mode (strategy s)
  {
    strat_ = s;
    launcher::log::trace_l2 (categories::cache {},
                             "reconciler strategy changed to {}",
                             static_cast<int> (s));
  }

  strategy reconciler::
  mode () const noexcept
  {
    return strat_;
  }

  void reconciler::
  progress (progress_cb cb)
  {
    cb_ = move (cb);
  }

  bool reconciler::
  outdated (component_type c, const string& t) const
  {
    auto v (db_.version (c));

    // A component is outdated if it has no recorded version or if the
    // recorded tag doesn't match the target tag.
    //
    bool o (!v || v->tag () != t);

    launcher::log::debug (categories::cache {},
                          "component {} outdated check vs '{}': {}",
                          static_cast<int> (c),
                          t,
                          o);
    return o;
  }

  optional<string> reconciler::
  version (component_type c) const
  {
    auto v (db_.version (c));
    return v ? make_optional (v->tag ()) : nullopt;
  }

  file_state reconciler::
  stat (const fs::path& p) const
  {
    string k (key (p));
    auto f (db_.find (k));

    // If we don't have a record of this file in the database, its state
    // is completely unknown to us. We can't verify it without a full hash.
    //
    if (!f)
    {
      launcher::log::trace_l3 (
        categories::cache {},
        "stat missing db record for {}, returning unknown",
        p.string ());
      return file_state::unknown;
    }

    return stat (p, *f);
  }

  file_state reconciler::
  stat (const fs::path& p, const cached_file& f) const
  {
    // The file might have been deleted from the disk by the user or some
    // other process.
    //
    if (!exists_quiet (p))
    {
      launcher::log::trace_l3 (categories::cache {},
                               "stat: file missing from disk: {}",
                               p.string ());
      return file_state::missing;
    }

    // Check if the file's metadata (like mtime or size) still matches what
    // we have in the database. If not, it's stale.
    //
    if (!match (p, f))
    {
      launcher::log::trace_l3 (categories::cache {},
                               "stat: file metadata stale: {}",
                               p.string ());
      return file_state::stale;
    }

    launcher::log::trace_l3 (categories::cache {},
                             "stat: file valid: {}",
                             p.string ());
    return file_state::valid;
  }

  vector<pair<cached_file, file_state>> reconciler::
  audit (component_type c) const
  {
    launcher::log::info (categories::cache {},
                         "starting heavy audit for component {}",
                         static_cast<int> (c));

    auto fs (db_.files (c));
    vector<pair<cached_file, file_state>> r;
    r.reserve (fs.size ());

    // Walk through all known files for this component and re-evaluate their
    // state against the filesystem.
    //
    for (const auto& f : fs)
    {
      fs::path p (f.path ());
      r.emplace_back (f, stat (p, f));
    }

    launcher::log::debug (categories::cache {},
                          "audit complete for component {}, checked {} files",
                          static_cast<int> (c),
                          r.size ());
    return r;
  }

  struct hash_task
  {
    fs::path p;
    string e;
    string k;
    component_type c;
    bool m;
    int64_t mt;
    uint64_t s;
    string err;
  };

  namespace
  {
    void
    run_hashes (vector<hash_task>& ts,
                function<void (const string&, size_t, size_t)> cb)
    {
      if (ts.empty ())
        return;

      // Determine a reasonable number of workers. Hardware concurrency is a
      // good default, but it can return 0 on some platforms, so we provide
      // a fallback.
      //
      unsigned int n (thread::hardware_concurrency ());
      if (n == 0)
        n = 4;

      launcher::log::trace_l2 (
        categories::cache {},
        "spinning up thread pool with {} workers for {} hash tasks",
        n,
        ts.size ());

      asio::thread_pool pl (n);
      atomic<size_t> d (0);
      size_t tot (ts.size ());
      latch l (static_cast<ptrdiff_t> (tot));

      for (auto& t : ts)
      {
        asio::post (pl,[&t, &d, tot, &l, &cb] ()
        {
          try
          {
            // We only bother hashing if the file actually exists and we have
            // an expected hash to compare against. Otherwise it's a guaranteed
            // mismatch.
            //
            if (exists_quiet (t.p) && !t.e.empty ())
            {
              string a (compute_blake3 (t.p));
              string e (t.e);

              e.erase (remove_if (e.begin (),
                                  e.end (),
                                  [] (unsigned char c)
              {
                return isspace (c);
              }), e.end ());

              bool m (a.size () == e.size () && !a.empty ());
              if (m)
              {
                for (size_t i (0); i < a.size (); ++i)
                {
                  if (tolower ((unsigned char) a[i]) !=
                      tolower ((unsigned char) e[i]))
                  {
                    m = false;
                    break;
                  }
                }
              }
              t.m = m;

              if (t.m)
              {
                t.mt = get_file_mtime (t.p);
                t.s = size_quiet (t.p);
              }
              else
              {
                launcher::log::debug (
                  categories::cache {},
                  "hash mismatch for {}: expected '{}', got '{}'",
                  t.p.string (),
                  e,
                  a);
              }
            }
            else
            {
              t.m = false;
            }
          }
          catch (const exception& e)
          {
            launcher::log::warning (categories::cache {},
                                    "exception during parallel hash for {}: {}",
                                    t.p.string (),
                                    e.what ());
            t.m = false;
            t.err = e.what ();
          }
          catch (...)
          {
            launcher::log::warning (
              categories::cache {},
              "unknown exception during parallel hash for {}",
              t.p.string ());
            t.m = false;
            t.err = "unknown exception during hashing";
          }

          // Update progress if a callback was provided.
          //
          size_t c (++d);
          if (cb)
            cb ("Verifying", c, tot);

          l.count_down ();
        });
      }

      l.wait ();
      pl.join ();
      launcher::log::trace_l2 (categories::cache {},
                               "all hash tasks completed");
    }
  }

  vector<reconcile_item> reconciler::
  plan (const manifest& m, component_type c, const string& v)
  {
    // The reconcile plan consists of tasks for archives and standalone files.
    // We generate both plans and stitch them together into a single action
    // list for the manifest coordinator.
    //
    launcher::log::trace_l1 (
      categories::cache {},
      "generating reconcile plan for component {} (target version {})",
      static_cast<int> (c),
      v);

    vector<reconcile_item> r;

    auto as (plan_archives (m.archives, c, v));
    r.insert (r.end (),
              make_move_iterator (as.begin ()),
              make_move_iterator (as.end ()));

    auto fs (plan_files (m.files, c, v));
    r.insert (r.end (),
              make_move_iterator (fs.begin ()),
              make_move_iterator (fs.end ()));

    launcher::log::info (categories::cache {},
                         "reconcile plan generated with {} items",
                         r.size ());
    return r;
  }

  vector<reconcile_item> reconciler::
  plan_archives (const vector<manifest_archive>& as,
                 component_type c,
                 const string& v)
  {
    launcher::log::trace_l2 (categories::cache {},
                             "planning {} archives",
                             as.size ());
    vector<reconcile_item> r;

    struct link_info { size_t i; fs::path p; string h; string k; };
    struct file_info { size_t i; fs::path p; string h; string k; };

    vector<link_info> ls;
    vector<file_info> fs;

    struct state { bool skip; bool hl; bool ok; bool dl; };
    vector<state> ss (as.size ());

    // First pass: evaluate the current disk and cache state of each archive
    // and its associated files. We try to determine if we can skip it, if
    // it needs downloading, or if we need to schedule it for hash
    // verification.
    //
    for (size_t i (0); i < as.size (); ++i)
    {
      const auto& a (as[i]);
      auto& s (ss[i]);

      s.skip = false;
      s.hl = !a.files.empty ();
      s.ok = true;
      s.dl = false;

      // An archive without a URL is useless to us. We simply skip it.
      //
      if (a.url.empty ())
      {
        launcher::log::trace_l3 (categories::cache {},
                                 "skipping archive {} (no URL provided)",
                                 a.name);
        s.skip = true;
        continue;
      }

      if (s.hl)
      {
        // For exploded archives, we check each inner file. If any file is
        // stale or missing, the whole archive's integrity is compromised.
        //
        for (const auto& f : a.files)
        {
          fs::path p (root_ / f.path);
          string k (key (p));
          auto cf (db_.find (k));

          if (cf && cf->version () == v && stat (p, *cf) == file_state::valid)
          {
            // Valid.
          }
          else if (exists_quiet (p) && !f.hash.value.empty ())
          {
            ls.push_back ({i, p, f.hash.value, k});
          }
          else
          {
            launcher::log::trace_l3 (
              categories::cache {},
              "exploded archive {} inner file missing or stale without hash: {}",
              a.name,
              p.string ());
            s.ok = false;
          }
        }
      }
      else
      {
        // For standalone blob archives, the logic is simpler. We just
        // verify the blob itself against the database.
        //
        fs::path p (path (a));
        string k (key (p));
        auto cf (db_.find (k));

        if (cf && cf->version () == v && stat (p, *cf) == file_state::valid)
        {
          s.skip = true;
        }
        else if (exists_quiet (p) && !a.hash.value.empty ())
        {
          if (cf)
          {
            launcher::log::trace_l3 (
              categories::cache {},
              "blob archive {} version mismatch or stale, checking hash",
              a.name);
          }
          fs.push_back ({i, p, a.hash.value, k});
        }
        else
        {
          launcher::log::trace_l3 (categories::cache {},
                                   "blob archive {} missing from disk/db or missing hash",
                                   a.name);
          s.dl = true;
        }
      }
    }

    // If we identified files that exist but aren't tracked yet, we run a
    // parallel hash check. This helps us adopt existing files into the
    // cache without redownloading them.
    //
    if (!ls.empty () || !fs.empty ())
    {
      vector<hash_task> ts;
      ts.reserve (ls.size () + fs.size ());

      for (const auto& l : ls)
        ts.push_back ({l.p, l.h, l.k, c, false, 0, 0, ""});

      for (const auto& f : fs)
        ts.push_back ({f.p, f.h, f.k, c, false, 0, 0, ""});

      run_hashes (ts, cb_);

      auto i (ts.begin ());

      for (const auto& l : ls)
      {
        const auto& t (*i++);

        if (t.m)
        {
          launcher::log::trace_l3 (categories::cache {},
                                   "adopting existing file into db: {}",
                                   l.k);
          db_.store (cached_file (l.k, t.mt, v, c, t.s, l.h));
        }
        else
        {
          launcher::log::debug (
            categories::cache {},
            "hash mismatch for {}, invalidating parent archive",
            l.p.string ());
          ss[l.i].ok = false;
        }
      }

      for (const auto& f : fs)
      {
        const auto& t (*i++);
        auto& s (ss[f.i]);

        if (t.m)
        {
          launcher::log::trace_l3 (categories::cache {},
                                   "adopting existing blob archive into db: {}",
                                   f.k);
          db_.store (cached_file (f.k, t.mt, v, c, t.s, f.h));
          s.skip = true;
        }
        else
        {
          s.dl = true;
        }
      }
    }

    // Finally, construct the reconciliation plan items based on the state we
    // determined in the first pass and updated in the hashing phase.
    //
    for (size_t i (0); i < as.size (); ++i)
    {
      const auto& a (as[i]);
      const auto& s (ss[i]);

      if (s.skip)
        continue;

      bool dl (s.dl);

      if (s.hl && !s.ok)
        dl = true;

      if (dl)
      {
        reconcile_item ri;
        ri.action = reconcile_action::download;
        ri.path = s.hl ? (root_ / a.name).string () : path (a).string ();
        ri.url = a.url;
        ri.expected_hash = a.hash.value;
        ri.expected_size = a.size;
        ri.component = c;
        ri.version = v;

        launcher::log::trace_l3 (categories::cache {},
                                 "archive item added to reconcile plan: {}",
                                 ri.path);
        r.push_back (move (ri));
      }
    }

    return r;
  }

  vector<reconcile_item> reconciler::
  plan_files (const vector<manifest_file>& fs,
              component_type c,
              const string& v)
  {
    launcher::log::trace_l2 (categories::cache {},
                             "planning {} standalone files",
                             fs.size ());
    vector<reconcile_item> r;
    size_t i (0);

    // Iterate through standalone files. Similar to archives, we want to skip
    // anything that is already managed by an archive or is merely a manifest
    // artifact (like the update JSON itself).
    //
    for (const auto& f : fs)
    {
      report ("Checking " + f.path, ++i, fs.size ());

      if (f.archive_name)
        continue;
      if (f.path.ends_with ("update.json"))
        continue;

      fs::path p (path (f));
      string k (key (p));
      auto cf (db_.find (k));
      bool dl (false);

      if (cf && cf->version () == v && stat (p, *cf) == file_state::valid)
      {
        // Valid, do nothing.
      }
      else if (exists_quiet (p) && !f.hash.value.empty ())
      {
        if (cf)
        {
          launcher::log::trace_l3 (
            categories::cache {},
            "file {} version mismatch or stale, checking hash",
            f.path);
        }

        report ("Hashing " + f.path, i, fs.size ());

        try
        {
          string a (compute_blake3 (p));

          string expected = f.hash.value;
          expected.erase(remove_if(expected.begin(), expected.end(),
              [](unsigned char ch){ return std::isspace(ch); }), expected.end());

          bool match = (a.size() == expected.size() && !a.empty());
          if (match) {
              for (size_t idx = 0; idx < a.size(); ++idx) {
                  if (std::tolower((unsigned char)a[idx]) != std::tolower((unsigned char)expected[idx])) {
                      match = false;
                      break;
                  }
              }
          }

          if (match)
          {
            launcher::log::trace_l3 (
              categories::cache {},
              "file {} matches expected hash, adopting to db",
              f.path);
            db_.store (cached_file (k,
                                    get_file_mtime (p),
                                    v,
                                    c,
                                    fs::file_size (p),
                                    f.hash.value));
          }
          else
          {
            launcher::log::debug (
              categories::cache {},
              "hash mismatch on adoption attempt for {}: expected '{}', got '{}'",
              f.path, expected, a);
            dl = true;
          }
        }
        catch (const exception& e)
        {
          launcher::log::warning (categories::cache {},
                                  "exception during hash verification for {}: {}",
                                  f.path, e.what());
          dl = true;
        }
        catch (...)
        {
          launcher::log::warning (categories::cache {},
                                  "unknown exception during hash verification for {}",
                                  f.path);
          dl = true;
        }
      }
      else
      {
        if (!cf)
        {
          launcher::log::trace_l3 (categories::cache {},
                                   "file {} missing from disk and db",
                                   f.path);
        }
        else
        {
          launcher::log::trace_l3 (categories::cache {},
                                   "file {} missing from disk or missing hash",
                                   f.path);
        }
        dl = true;
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
        r.push_back (move (ri));

        launcher::log::trace_l3 (categories::cache {},
                                 "file item added to reconcile plan: {}",
                                 ri.path);
      }
    }

    return r;
  }

  reconcile_summary reconciler::
  summarize (const vector<reconcile_item>& is) const
  {
    launcher::log::trace_l3 (categories::cache {},
                             "generating summary for {} items",
                             is.size ());
    reconcile_summary s;

    // Accumulate metrics for the UI or logging layer.
    //
    for (const auto& i : is)
    {
      switch (i.action)
      {
        case reconcile_action::download:
          s.downloads_required++, s.bytes_to_download += i.expected_size;
          break;

        case reconcile_action::verify:
          s.files_stale++;
          break;
        case reconcile_action::remove:
          s.files_unknown++;
          break;
        case reconcile_action::none:
          s.files_valid++;
          break;
      }
    }

    return s;
  }

  void reconciler::
  track (const fs::path& p, component_type c, const string& v, const string& h)
  {
    if (!exists_quiet (p))
    {
      launcher::log::warning (categories::cache {},
                              "attempted to track non-existent file: {}",
                              p.string ());
      return;
    }

    // Record the file's current state so we can quickly verify it in
    // future runs without rehashing.
    //
    try
    {
      launcher::log::trace_l3 (categories::cache {},
                               "tracking file: {}",
                               p.string ());
      db_.store (
        cached_file (key (p), get_file_mtime (p), v, c, fs::file_size (p), h));
    }
    catch (const exception& e)
    {
      launcher::log::warning (categories::cache {},
                              "exception tracking file {}: {}",
                              p.string (),
                              e.what ());
    }
    catch (...)
    {
      launcher::log::warning (categories::cache {},
                              "unknown exception tracking file {}",
                              p.string ());
    }
  }

  void reconciler::
  track (const vector<fs::path>& ps, component_type c, const string& v)
  {
    launcher::log::trace_l2 (categories::cache {},
                             "batch tracking {} files",
                             ps.size ());
    vector<cached_file> cfs;
    cfs.reserve (ps.size ());

    for (const auto& p : ps)
    {
      if (!exists_quiet (p))
        continue;
      try
      {
        cfs.emplace_back (key (p),
                          get_file_mtime (p),
                          v,
                          c,
                          fs::file_size (p),
                          "");
      }
      catch (...)
      {
      }
    }

    // Doing this in a batch minimizes database transaction overhead.
    //
    if (!cfs.empty ())
    {
      launcher::log::trace_l3 (categories::cache {},
                               "storing {} stat'd files to db",
                               cfs.size ());
      db_.store (cfs);
    }
  }

  void reconciler::
  stamp (component_type c, const string& t)
  {
    launcher::log::info (categories::cache {},
                         "stamping component {} with version tag {}",
                         static_cast<int> (c),
                         t);
    db_.version (c, t);
  }

  void reconciler::
  forget (const fs::path& p)
  {
    launcher::log::trace_l3 (categories::cache {},
                             "forgetting file from db: {}",
                             p.string ());
    db_.erase (key (p));
  }

  vector<string> reconciler::
  clean (const manifest& m, component_type c)
  {
    launcher::log::info (categories::cache {},
                         "cleaning orphaned files for component {}",
                         static_cast<int> (c));

    vector<string> os;
    vector<string> es;

    for (const auto& a : m.archives)
    {
      fs::path p (path (a));
      string ext (p.extension ().string ());
      transform (ext.begin (),
                 ext.end (),
                 ext.begin (),
                 [] (unsigned char ch)
      {
        return tolower (ch);
      });

      // If it's a zip file, we expect the inner files to be in the cache.
      //
      if (ext == ".zip")
      {
        for (const auto& f : a.files)
          es.push_back (key (path (f)));
      }
      else
      {
        // For standalone blobs (like .iwd files), the archive itself stays on
        // disk.
        //
        es.push_back (key (p));
      }
    }

    for (const auto& f : m.files)
      if (!f.archive_name)
        es.push_back (key (path (f)));

    sort (es.begin (), es.end ());

    auto cfs (db_.files (c));

    for (const auto& f : cfs)
    {
      if (!binary_search (es.begin (), es.end (), f.path ()))
        os.push_back (f.path ());
    }

    launcher::log::debug (categories::cache {},
                          "found {} orphaned database entries",
                          os.size ());

    // If auto-pruning is enabled, we immediately strip the orphans from
    // our tracking database.
    //
    if (auto_prune && !os.empty ())
    {
      launcher::log::trace_l2 (categories::cache {},
                               "auto-pruning orphans from db");
      db_.erase (os);
    }

    return os;
  }

  fs::path reconciler::
  path (const manifest_file& f) const
  {
    return manifest_coordinator::resolve_path (f, root_);
  }

  fs::path reconciler::
  path (const manifest_archive& a) const
  {
    return manifest_coordinator::resolve_path (a, root_);
  }

  string reconciler::
  key (const fs::path& p) const
  {
    error_code ec;
    fs::path can (fs::weakly_canonical (p, ec));
    string r (ec ? p.string () : can.string ());

    launcher::log::trace_l3 (categories::cache {},
                             "generated key for path {}: {}",
                             p.string (),
                             r);
    return r;
  }

  void reconciler::
  report (const string& m, size_t cur, size_t tot)
  {
    if (cb_)
      cb_ (m, cur, tot);
  }

  bool reconciler::
  match (const fs::path& p, const cached_file& f) const
  {
    try
    {
      launcher::log::trace_l3 (
        categories::cache {},
        "matching file {} (expected mtime: {}, size: {})",
        p.string (),
        f.mtime (),
        f.size ());

      // The mtime is our primary heuristic. If it differs, the file was
      // likely touched or replaced.
      //
      if (get_file_mtime (p) != f.mtime ())
      {
        launcher::log::trace_l3 (categories::cache {},
                                 "mtime mismatch for {}",
                                 p.string ());
        return false;
      }

      // Depending on the configured strategy, we might also verify the file
      // size. In pure timestamp mode, we skip this to save I/O overhead.
      //
      if (strat_ == strategy::mixed || strat_ == strategy::hash)
      {
        if (fs::file_size (p) != f.size ())
        {
          launcher::log::trace_l3 (categories::cache {},
                                   "size mismatch for {}",
                                   p.string ());
          return false;
        }
      }

      launcher::log::trace_l3 (categories::cache {},
                               "match found for {}",
                               p.string ());
      return true;
    }
    catch (const exception& e)
    {
      launcher::log::warning (categories::cache {},
                              "exception during match for {}: {}",
                              p.string (),
                              e.what ());
      return false;
    }
    catch (...)
    {
      launcher::log::warning (categories::cache {},
                              "unknown exception during match for {}",
                              p.string ());
      return false;
    }
  }
}
