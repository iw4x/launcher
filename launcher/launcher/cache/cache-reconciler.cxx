// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/cache/cache-reconciler.hxx>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <exception>
#include <latch>
#include <mutex>
#include <thread>
#include <unordered_map>
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
    // Return true if the path exists and false if it doesn't or cannot be
    // examined. Note that missing files are normal during reconciliation, so
    // we don't want to throw.
    //
    bool
    exists_quiet (const fs::path& p)
    {
      error_code ec;
      return fs::exists (p, ec);
    }

    // Return the size of the file or 0 if it cannot be examined, which is
    // good enough for the size checks below.
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
      root_ (fs::weakly_canonical (r)),
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

    // Without a record we know nothing about the file.
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
    // Note that the file can be gone even though we have a record of it if
    // it was removed behind our back.
    //
    if (!exists_quiet (p))
    {
      launcher::log::trace_l3 (categories::cache {},
                               "stat: file missing from disk: {}",
                               p.string ());
      return file_state::missing;
    }

    // A file whose metadata differs from the record was written behind our
    // back and so is stale.
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
    // Hash the files of the tasks in parallel, setting the match flag of each
    // task and, if the file matches, its current metadata. Call the callback,
    // if any, as each task completes.
    //
    void
    run_hashes (vector<hash_task>& ts,
                function<void (const string&, size_t, size_t)> cb)
    {
      if (ts.empty ())
        return;

      // Use a worker per hardware thread. Note that hardware_concurrency()
      // can return 0, in which case we use 4.
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
            // Without the file or an expected hash there is nothing to match.
            //
            if (exists_quiet (t.p) && !t.e.empty ())
            {
              blake3_hash actual (blake3_hash::of_file (t.p));
              blake3_hash expected (t.e);

              t.m = (!actual.empty () && actual == expected);

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
                  expected.string (),
                  actual.string ());
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
    launcher::log::trace_l1 (
      categories::cache {},
      "generating reconcile plan for component {} (target version {})",
      static_cast<int> (c),
      v);

    // Load the records of the component up front, which saves the planners
    // a database query per file.
    //
    cache_map cm;
    {
      auto fs (db_.files (c));
      cm.reserve (fs.size ());
      for (auto& f : fs)
        cm.emplace (f.path (), move (f));
    }

    vector<reconcile_item> r;

    auto as (plan_archives (m.archives, c, v, cm));
    r.insert (r.end (),
              make_move_iterator (as.begin ()),
              make_move_iterator (as.end ()));

    auto fs (plan_files (m.files, c, v, cm));
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
                 const string& v,
                 const cache_map& cm)
  {
    launcher::log::trace_l2 (categories::cache {},
                             "planning {} archives",
                             as.size ());
    vector<reconcile_item> r;

    struct link_info { size_t i; fs::path p; string h; string k; uint64_t es; };
    struct file_info { size_t i; fs::path p; string h; string k; uint64_t es; };

    vector<link_info> ls;
    vector<file_info> fs;

    struct state { bool skip; bool hl; bool ok; bool dl; };
    vector<state> ss (as.size ());

    // First see which archives are intact and which need downloading. For
    // the rest we will only know after hashing some of their files.
    //
    for (size_t i (0); i < as.size (); ++i)
    {
      const auto& a (as[i]);
      auto& s (ss[i]);

      s.skip = false;
      s.hl = !a.files.empty ();
      s.ok = true;
      s.dl = false;

      if (s.hl)
      {
        // An extracted archive is intact if all its files are.
        //
        for (const auto& f : a.files)
        {
          fs::path p (path (f));
          string k (key (p));
          auto it (cm.find (k));

          if (it != cm.end () &&
              it->second.version () == v &&
              stat (p, it->second) == file_state::valid)
          {
            // Valid.
          }
          else if (exists_quiet (p) && !f.hash.value.empty ())
          {
            // If the size doesn't match, there is no point in hashing.
            //
            uint64_t ds (size_quiet (p));
            if (f.size != 0 && ds != f.size)
            {
              launcher::log::trace_l3 (
                categories::cache {},
                "size mismatch for {}: expected {}, got {}",
                p.string (), f.size, ds);
              s.ok = false;
            }
            else
            {
              ls.push_back ({i, p, f.hash.value, k, f.size});
            }
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
        // An archive that stays as it is (an .iwd, for example) is checked
        // as a file.
        //
        fs::path p (path (a));
        string k (key (p));
        auto it (cm.find (k));

        if (it != cm.end () &&
            it->second.version () == v &&
            stat (p, it->second) == file_state::valid)
        {
          s.skip = true;
        }
        else if (exists_quiet (p) && !a.hash.value.empty ())
        {
          if (it != cm.end ())
          {
            launcher::log::trace_l3 (
              categories::cache {},
              "blob archive {} version mismatch or stale, checking hash",
              a.name);
          }

          // As above.
          //
          uint64_t ds (size_quiet (p));
          if (a.size != 0 && ds != a.size)
          {
            launcher::log::trace_l3 (
              categories::cache {},
              "blob archive {} size mismatch: expected {}, got {}",
              a.name, a.size, ds);
            s.dl = true;
          }
          else
          {
            fs.push_back ({i, p, a.hash.value, k, a.size});
          }
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

    // Hash the files that exist without an up-to-date record. Those that
    // match are adopted, which saves us downloading them again.
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

      // Store the records of the adopted files in a single transaction.
      //
      vector<cached_file> adoptions;
      auto i (ts.begin ());

      for (const auto& l : ls)
      {
        const auto& t (*i++);

        if (t.m)
        {
          launcher::log::trace_l3 (categories::cache {},
                                   "adopting existing file into db: {}",
                                   l.k);
          adoptions.emplace_back (l.k, t.mt, v, c, t.s, l.h);
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
          adoptions.emplace_back (f.k, t.mt, v, c, t.s, f.h);
          s.skip = true;
        }
        else
        {
          s.dl = true;
        }
      }

      if (!adoptions.empty ())
        db_.store (adoptions);
    }

    // Plan the download of every archive that is not intact.
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
        ri.path = path (a).string ();
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
              const string& v,
              const cache_map& cm)
  {
    launcher::log::trace_l2 (categories::cache {},
                             "planning {} standalone files",
                             fs.size ());
    vector<reconcile_item> r;

    struct file_entry
    {
      size_t mi;       // Manifest index.
      fs::path p;      // Resolved path.
      string k;        // DB key.
      string h;        // Expected hash.
      uint64_t es;     // Expected size.
    };

    vector<file_entry> to_hash;
    vector<size_t> to_download;

    size_t i (0);

    for (size_t mi (0); mi < fs.size (); ++mi)
    {
      const auto& f (fs[mi]);

      report ("Checking " + f.path, ++i, fs.size ());

      if (f.archive_name)
        continue;
      if (f.path.ends_with ("update.json"))
        continue;

      fs::path p (path (f));
      string k (key (p));
      auto it (cm.find (k));

      if (it != cm.end () &&
          it->second.version () == v &&
          stat (p, it->second) == file_state::valid)
      {
        // Valid.
      }
      else if (exists_quiet (p) && !f.hash.value.empty ())
      {
        if (it != cm.end ())
        {
          launcher::log::trace_l3 (
            categories::cache {},
            "file {} version mismatch or stale, checking hash",
            f.path);
        }

        // As in plan_archives().
        //
        uint64_t ds (size_quiet (p));
        if (f.size != 0 && ds != f.size)
        {
          launcher::log::trace_l3 (
            categories::cache {},
            "file {} size mismatch: expected {}, got {}",
            f.path, f.size, ds);
          to_download.push_back (mi);
        }
        else
        {
          to_hash.push_back ({mi, p, k, f.hash.value, f.size});
        }
      }
      else
      {
        if (it == cm.end ())
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
        to_download.push_back (mi);
      }
    }

    // Hash the files that exist without an up-to-date record (as in
    // plan_archives()).
    //
    if (!to_hash.empty ())
    {
      vector<hash_task> ts;
      ts.reserve (to_hash.size ());

      for (const auto& e : to_hash)
        ts.push_back ({e.p, e.h, e.k, c, false, 0, 0, ""});

      run_hashes (ts, cb_);

      // Adopt the matching files and download the rest.
      //
      vector<cached_file> adoptions;

      for (size_t j (0); j < to_hash.size (); ++j)
      {
        const auto& e (to_hash[j]);
        const auto& t (ts[j]);

        if (t.m)
        {
          launcher::log::trace_l3 (
            categories::cache {},
            "file {} matches expected hash, adopting to db",
            fs[e.mi].path);
          adoptions.emplace_back (e.k, t.mt, v, c, t.s, e.h);
        }
        else
        {
          launcher::log::debug (
            categories::cache {},
            "hash mismatch on adoption attempt for {}: expected '{}'",
            fs[e.mi].path, e.h);
          to_download.push_back (e.mi);
        }
      }

      if (!adoptions.empty ())
        db_.store (adoptions);
    }

    // Plan the downloads.
    //
    for (size_t mi : to_download)
    {
      const auto& f (fs[mi]);
      reconcile_item ri;
      ri.action = reconcile_action::download;
      ri.path = path (f).string ();
      ri.expected_hash = f.hash.value;
      ri.expected_size = f.size;
      ri.component = c;
      ri.version = v;
      r.push_back (move (ri));

      launcher::log::trace_l3 (categories::cache {},
                               "file item added to reconcile plan: {}",
                               ri.path);
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

    // Store the records in a single transaction.
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

      // A ZIP archive is extracted, so we have records of its files.
      //
      if (ext == ".zip")
      {
        for (const auto& f : a.files)
          es.push_back (key (path (f)));
      }
      else
      {
        // Other archives stay as they are and have records of their own.
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
    // Note that the paths we get are based on the canonical root (see
    // path()), so lexical normalization is enough.
    //
    string r (p.lexically_normal ().string ());

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

      // A different modification time means the file was written since we
      // recorded it.
      //
      if (get_file_mtime (p) != f.mtime ())
      {
        launcher::log::trace_l3 (categories::cache {},
                                 "mtime mismatch for {}",
                                 p.string ());
        return false;
      }

      // Unless we only trust the modification time, compare the size as
      // well.
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
