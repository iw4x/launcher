#include <launcher/launcher-cache.hxx>

#include <iostream>
#include <unordered_map>

#include <launcher/launcher-download.hxx>
#include <launcher/launcher-progress.hxx>

using namespace std;

namespace launcher
{
  ostream&
  operator<< (ostream& os, cache_status s)
  {
    switch (s)
    {
      case cache_status::up_to_date:      return os << "up_to_date";
      case cache_status::update_required: return os << "update_required";
      case cache_status::update_applied:  return os << "update_applied";
      case cache_status::check_failed:    return os << "check_failed";
      case cache_status::update_failed:   return os << "update_failed";
    }
    return os;
  }

  cache_result::
  cache_result ()
    : status (cache_status::up_to_date)
  {
  }

  cache_result::
  cache_result (cache_status s)
    : status (s)
  {
  }

  cache_result::
  cache_result (cache_status s, reconcile_summary sum)
    : status (s),
      summary (move (sum))
  {
  }

  cache_result::
  cache_result (cache_status s, string e)
    : status (s),
      error (move (e))
  {
  }

  bool cache_result::
  ok () const noexcept
  {
    return status == cache_status::up_to_date ||
           status == cache_status::update_applied;
  }

  cache_result::
  operator bool () const noexcept
  {
    return ok ();
  }

  cache_coordinator::
  cache_coordinator (asio::io_context& ioc, const fs::path& d)
    : ctx_ (ioc),
      dir_ (d),
      db_ (make_unique<database_type> (d)),
      rec_ (make_unique<reconciler_type> (*db_, d)),
      dl_ (nullptr),
      prog_ (nullptr),
      gh_ (nullptr)
  {
  }

  void cache_coordinator::
  set_download_coordinator (download_coordinator* d)
  {
    dl_ = d;
  }

  void cache_coordinator::
  set_progress_coordinator (progress_coordinator* p)
  {
    prog_ = p;
  }

  void cache_coordinator::
  set_github_coordinator (github_coordinator* g)
  {
    gh_ = g;
  }

  void cache_coordinator::
  set_progress_callback (progress_callback cb)
  {
    cb_ = move (cb);

    if (rec_)
      rec_->progress (cb_);
  }

  void cache_coordinator::
  set_strategy (strategy s)
  {
    if (rec_)
      rec_->mode (s);
  }

  strategy cache_coordinator::
  get_strategy () const noexcept
  {
    return rec_ ? rec_->mode () : strategy::mtime;
  }

  bool cache_coordinator::
  outdated (component_type c, const string& t) const
  {
    return rec_->outdated (c, t);
  }

  optional<string> cache_coordinator::
  version (component_type c) const
  {
    return rec_->version (c);
  }

  file_state cache_coordinator::
  stat (const fs::path& p) const
  {
    return rec_->stat (p);
  }

  vector<pair<cached_file, file_state>> cache_coordinator::
  audit (component_type c) const
  {
    return rec_->audit (c);
  }

  vector<reconcile_item> cache_coordinator::
  plan (const manifest& m,
        component_type c,
        const string& v)
  {
    return rec_->plan (m, c, v);
  }

  reconcile_summary cache_coordinator::
  summarize (const vector<reconcile_item>& is) const
  {
    return rec_->summarize (is);
  }

  cache_result cache_coordinator::
  check (const manifest& m,
         component_type c,
         const string& v)
  {
    // Generate the plan but don't act on it. Use this to inform the user if
    // an update is pending without actually touching the disk.
    //
    auto is (rec_->plan (m, c, v));
    auto s (rec_->summarize (is));

    if (s.up_to_date ())
      return cache_result (cache_status::up_to_date, s);

    return cache_result (cache_status::update_required, s);
  }

  asio::awaitable<cache_result> cache_coordinator::
  sync (const manifest& m,
        component_type c,
        const string& v)
  {
    // First, see what needs to be done.
    //
    auto is (rec_->plan (m, c, v));
    auto s (rec_->summarize (is));

    // If the summary says we are good, then we are good.
    //
    if (s.up_to_date ())
      co_return cache_result (cache_status::up_to_date, s);

    // Otherwise, handover to execute() to handle the heavy lifting (downloads
    // and DB updates).
    //
    co_return co_await execute (is, c, v);
  }

  asio::awaitable<cache_result> cache_coordinator::
  smart_sync (const manifest& m,
              component_type c,
              const string& t)
  {
    // The idea here is to avoid the full cost of reconciliation (planning) if
    // we think we are already on the right version.
    //
    if (!outdated (c, t))
    {
      // The DB says we have the right version, but the user might have messed
      // with the files manually. So do a quick mtime scan to validate the
      // physical state.
      //
      auto fs (audit (c));
      bool v (true);

      for (const auto& [f, s] : fs)
      {
        if (s != file_state::valid)
        {
          v = false;
          break;
        }
      }

      if (v)
        co_return cache_result (cache_status::up_to_date);
    }

    // If the tag doesn't match or the files look suspicious, fall back to the
    // full sync.
    //
    co_return co_await sync (m, c, t);
  }

  asio::awaitable<cache_result> cache_coordinator::
  sync_all (const vector<tuple<manifest, component_type, string>>& is)
  {
    reconcile_summary total;
    vector<reconcile_item> ais; // All items.

    // Aggregate plans for all components so we can batch the downloads.
    //
    for (const auto& [m, c, v] : is)
    {
      auto cis (rec_->plan (m, c, v));
      auto s (rec_->summarize (cis));

      total.files_valid += s.files_valid;
      total.files_stale += s.files_stale;
      total.files_missing += s.files_missing;
      total.downloads_required += s.downloads_required;
      total.bytes_to_download += s.bytes_to_download;

      ais.insert (ais.end (),
                  make_move_iterator (cis.begin ()),
                  make_move_iterator (cis.end ()));
    }

    if (total.up_to_date ())
      co_return cache_result (cache_status::up_to_date, total);

    if (dl_ == nullptr)
      co_return cache_result (cache_status::update_failed,
                              "download coordinator not configured");

    // Queue everything up.
    //
    for (const auto& i : ais)
    {
      if (i.action != reconcile_action::download)
        continue;

      if (i.url.empty ())
        continue;

      download_request r;
      r.urls.push_back (i.url);
      r.target = fs::path (i.path);
      r.name = fs::path (i.path).filename ().string ();
      r.expected_size = i.expected_size;

      dl_->queue_download (move (r));
    }

    // Run the batch.
    //
    co_await dl_->execute_all ();

    if (dl_->failed_count () > 0)
      co_return cache_result (cache_status::update_failed,
                              "some downloads failed");

    // If we survived the downloads, stamp all the versions in the DB.
    //
    for (const auto& [m, c, v] : is)
      rec_->stamp (c, v);

    co_return cache_result (cache_status::update_applied, total);
  }

  void cache_coordinator::
  track (const fs::path& p,
         component_type c,
         const string& v,
         const string& h)
  {
    rec_->track (p, c, v, h);
  }

  void cache_coordinator::
  track (const vector<fs::path>& ps,
         component_type c,
         const string& v)
  {
    rec_->track (ps, c, v);
  }

  void cache_coordinator::
  stamp (component_type c, const string& t)
  {
    rec_->stamp (c, t);
  }

  void cache_coordinator::
  forget (const fs::path& p)
  {
    rec_->forget (p);
  }

  vector<string> cache_coordinator::
  clean (const manifest& m, component_type c)
  {
    return rec_->clean (m, c);
  }

  void cache_coordinator::
  clear ()
  {
    db_->clear ();
  }

  void cache_coordinator::
  vacuum ()
  {
    db_->vacuum ();
  }

  bool cache_coordinator::
  check_integrity () const
  {
    return db_->check ();
  }

  cache_coordinator::database_type& cache_coordinator::
  database () noexcept
  {
    return *db_;
  }

  const cache_coordinator::database_type& cache_coordinator::
  database () const noexcept
  {
    return *db_;
  }

  cache_coordinator::reconciler_type& cache_coordinator::
  get_reconciler () noexcept
  {
    return *rec_;
  }

  const cache_coordinator::reconciler_type& cache_coordinator::
  get_reconciler () const noexcept
  {
    return *rec_;
  }

  const fs::path& cache_coordinator::
  install_directory () const noexcept
  {
    return dir_;
  }

  asio::awaitable<cache_result> cache_coordinator::
  execute (const vector<reconcile_item>& is,
           component_type c,
           const string& v)
  {
    if (dl_ == nullptr)
      co_return cache_result (cache_status::update_failed,
                              "download coordinator not configured");

    // Filter out items that don't need downloading.
    //
    vector<const reconcile_item*> ds;

    for (const auto& i : is)
    {
      if (i.action == reconcile_action::download && !i.url.empty ())
        ds.push_back (&i);
    }

    // Short-circuit if we only have local operations (which we assume
    // succeeded during planning/verification).
    //
    if (ds.empty ())
    {
      rec_->stamp (c, v);
      co_return cache_result (cache_status::up_to_date);
    }

    // We need to map the async tasks back to the reconcile items so we can
    // update the DB with hashes upon completion.
    //
    using task_ptr = shared_ptr<download_coordinator::task_type>;
    unordered_map<task_ptr, const reconcile_item*> tm;

    for (const auto* i : ds)
    {
      fs::path t (i->path);

      // Make sure the directory tree exists before we start pouring bytes.
      //
      if (t.has_parent_path ())
      {
        error_code ec;
        fs::create_directories (t.parent_path (), ec);
      }

      download_request r;
      r.urls.push_back (i->url);
      r.target = t;
      r.name = t.filename ().string ();
      r.expected_size = i->expected_size;

      auto task (dl_->queue_download (move (r)));
      tm[task] = i;

      // Wire up the progress UI.
      //
      if (prog_ != nullptr)
      {
        auto e (prog_->add_entry (r.name));
        e->metrics ().total_bytes.store (i->expected_size,
                                         memory_order_relaxed);

        task->on_progress = [this, e] (const download_progress& p)
        {
          prog_->update_progress (e, p.downloaded_bytes, p.total_bytes);
        };
      }
    }

    co_await dl_->execute_all ();

    // Check for failures and update cache for successes.
    //
    size_t failed (0);

    for (const auto& [t, i] : tm)
    {
      if (t->failed ())
      {
        ++failed;
        continue;
      }

      track (fs::path (i->path),
             i->component,
             i->version,
             i->expected_hash);
    }

    if (failed > 0)
      co_return cache_result (cache_status::update_failed,
                              std::to_string (failed) + " downloads failed");

    // All good.
    //
    rec_->stamp (c, v);

    reconcile_summary s;
    s.downloads_required = ds.size ();

    co_return cache_result (cache_status::update_applied, s);
  }
}
