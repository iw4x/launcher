#include <launcher/launcher-update.hxx>

#include <atomic>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

#include <launcher/version.hxx>

using namespace std;

namespace launcher
{
  update_coordinator::
  update_coordinator (asio::io_context& c)
    : ioc_ (c),
      discovery_ (make_unique<discovery_type> (c)),
      installer_ (make_unique<installer_type> (c))
  {
    // Wire up the installer's progress reporting to our internal handler
    // immediately. We want to make sure that if the installer starts doing
    // work (even during setup), we catch those signals.
    //
    installer_->set_progress_callback (
      [this] (update_state s, double p, const string& m)
    {
      report_progress (s, p, m);
    });
  }

  void update_coordinator::
  set_repository (string o, string r)
  {
    owner_ = move (o);
    repo_ = move (r);
  }

  void update_coordinator::
  set_current_version (const version_type& v)
  {
    current_version_ = v;
  }

  void update_coordinator::
  set_current_version (const string& s)
  {
    auto v (parse_launcher_version (s));

    if (!v)
      throw invalid_argument ("failed to parse version: " + s);

    current_version_ = *v;
  }

  void update_coordinator::
  set_token (string t)
  {
    discovery_->set_token (move (t));
  }

  void update_coordinator::
  set_include_prerelease (bool i)
  {
    discovery_->set_include_prerelease (i);
  }

  void update_coordinator::
  set_progress_callback (progress_callback_type cb)
  {
    progress_callback_ = move (cb);
  }

  void update_coordinator::
  set_completion_callback (completion_callback_type cb)
  {
    completion_callback_ = move (cb);
  }

  void update_coordinator::
  set_auto_restart (bool r)
  {
    auto_restart_ = r;
  }

  void update_coordinator::
  set_headless (bool h)
  {
    headless_ = h;
  }

  void update_coordinator::
  set_progress_coordinator (progress_coordinator* p)
  {
    progress_coord_ = p;
  }

  asio::awaitable<update_status> update_coordinator::
  check_for_updates ()
  {
    // We are entering the checking phase.
    //
    state_ = update_state::checking;

    try
    {
      // Ask the discovery backend. This might hit the network, so we await.
      //
      last_update_info_ = co_await discovery_->check_for_update (
        owner_, repo_, current_version_);

      if (last_update_info_.empty ())
      {
        state_ = update_state::idle;

        // In headless mode, we still want a textual confirmation that we are
        // clean, but we don't need the full UI song and dance.
        //
        if (headless_)
          cout << "launcher is up to date (" << current_version_ << ").\n";

        report_completion (update_status::up_to_date);
        co_return update_status::up_to_date;
      }

      // If we got here, we have a candidate.
      //
      state_ = update_state::idle;
      report_completion (update_status::update_available);
      co_return update_status::update_available;
    }
    catch (const exception& e)
    {
      // Network errors or parsing failures aren't fatal to the application,
      // but they do mean the update process stops here.
      //
      state_ = update_state::failed;
      report_completion (update_status::check_failed, e.what ());
      co_return update_status::check_failed;
    }
  }

  asio::awaitable<update_status> update_coordinator::
  check_and_update ()
  {
    auto s (co_await check_for_updates ());

    if (s == update_status::up_to_date)
      co_return s;

    if (s == update_status::check_failed)
    {
      if (!headless_)
        cerr << "warning: failed to check for launcher updates\n";
      co_return s;
    }

    // Inform the user. Even in headless mode, if an update is found, we want
    // to log it to stdout so logs capture the transition.
    //
    cout << "launcher update available: " << last_update_info_.version
         << " (current: " << current_version_ << ")\n";

    if (!headless_ && !last_update_info_.body.empty ())
      cout << "Release notes:\n" << last_update_info_.body << "\n\n";

    auto r (co_await install_update (last_update_info_));

    if (!r.success)
    {
      cerr << "error: update failed: " << r.error_message << "\n";
      state_ = update_state::failed;
      report_completion (update_status::check_failed, r.error_message);
      co_return update_status::check_failed;
    }

    // Try to restart into the new version. If this fails, we are in a
    // partially valid state (new binary on disk, old binary running). The
    // user will have to manually restart, but the data is safe.
    //
    if (restart ())
      co_return update_status::update_available;

    cerr << "warning: failed to restart launcher automatically\n";
    cout << "please restart the launcher manually.\n";

    co_return update_status::update_available;
  }

  asio::awaitable<update_result> update_coordinator::
  install_update (const info_type& i)
  {
    if (i.empty () || i.asset_url.empty ())
    {
      update_result r;
      r.error_message = "invalid update info";
      co_return r;
    }

    // If we have a progress coordinator, we need to create an entry for the
    // download.
    //
    // Note: We are capturing the entry `e` in the lambda by value
    // (shared_ptr) to keep it alive during the async download process.
    //
    shared_ptr<progress_entry> e;

    if (progress_coord_ != nullptr)
    {
      e = progress_coord_->add_entry (i.asset_name);
      e->metrics ().total_bytes.store (i.asset_size, memory_order_relaxed);

      installer_->set_progress_callback (
        [this, e] (update_state s, double p, const string& m)
      {
        // We need to translate the generic percentage coming from the
        // installer into specific byte counts required by the progress
        // coordinator.
        //
        if (s == update_state::downloading && e)
        {
          uint64_t c (static_cast<uint64_t> (
            p * e->metrics ().total_bytes.load (memory_order_relaxed)));

          progress_coord_->update_progress (
            e,
            c,
            e->metrics ().total_bytes.load (memory_order_relaxed));
        }

        // Forward to the general UI callback as well.
        //
        report_progress (s, p, m);
      });
    }

    auto r (co_await installer_->install (i));

    if (e && progress_coord_ != nullptr)
      progress_coord_->remove_entry (e);

    if (r.success)
    {
      state_ = update_state::completed;
      last_installed_path_ = r.installed_path;
    }
    else
    {
      state_ = update_state::failed;
    }

    co_return r;
  }

  bool update_coordinator::
  restart ()
  {
    // Determine the target executable.
    //
    // On some platforms (Windows), the installer might have moved the
    // currently running executable to a temporary location (e.g., .backup) to
    // allow writing the new one.
    //
    // Therefore, we should prefer `last_installed_path_` if it's set, as that
    // points to the fresh binary. Fallback to `current_executable_path` only
    // if we haven't installed anything or the path is weird.
    //
    fs::path t;
    if (!last_installed_path_.empty () && fs::exists (last_installed_path_))
      t = last_installed_path_;
    else
      t = installer_type::current_executable_path ();

    state_ = update_state::restarting;
    report_progress (update_state::restarting, 0.0, "Restarting...");

    return installer_->schedule_restart (t);
  }

  update_state update_coordinator::
  state () const noexcept
  {
    return state_;
  }

  const update_coordinator::info_type& update_coordinator::
  last_update_info () const noexcept
  {
    return last_update_info_;
  }

  const update_coordinator::version_type& update_coordinator::
  current_version () const noexcept
  {
    return current_version_;
  }

  bool update_coordinator::
  update_available () const noexcept
  {
    return !last_update_info_.empty () &&
           last_update_info_.version > current_version_;
  }

  update_coordinator::discovery_type& update_coordinator::
  discovery () noexcept
  {
    return *discovery_;
  }

  const update_coordinator::discovery_type& update_coordinator::
  discovery () const noexcept
  {
    return *discovery_;
  }

  update_coordinator::installer_type& update_coordinator::
  installer () noexcept
  {
    return *installer_;
  }

  const update_coordinator::installer_type& update_coordinator::
  installer () const noexcept
  {
    return *installer_;
  }

  void update_coordinator::
  report_progress (update_state s, double p, const string& m)
  {
    if (progress_callback_)
      progress_callback_ (s, p, m);
  }

  void update_coordinator::
  report_completion (update_status s, const string& e)
  {
    if (completion_callback_)
      completion_callback_ (s, last_update_info_, e);
  }

  unique_ptr<update_coordinator>
  make_update_coordinator (asio::io_context& c)
  {
    auto coord (make_unique<update_coordinator> (c));

    // Bake in the compiled version constants.
    //
    launcher_version v;
    v.major = HELLO_VERSION_MAJOR;
    v.minor = HELLO_VERSION_MINOR;
    v.patch = HELLO_VERSION_PATCH;

    // If we are running a pre-release build, we need to be specific about the
    // snapshot ID to upgrade to a newer pre-release or the final release
    // properly.
    //
#if HELLO_PRE_RELEASE
    auto p (parse_launcher_version (HELLO_VERSION_STR));
    if (p)
    {
      v.pre_release = p->pre_release;
      v.snapshot_sn = p->snapshot_sn;
      v.snapshot_id = p->snapshot_id;
    }
#endif

    coord->set_current_version (v);
    return coord;
  }

  string
  format_update_status (update_status s, const update_info& i)
  {
    ostringstream os;

    switch (s)
    {
      case update_status::up_to_date:
        os << "launcher is up to date";
        if (!i.empty ())
          os << " (" << i.version << ")";
        break;

      case update_status::update_available:
        os << "update available: " << i.version;
        if (i.prerelease)
          os << " (pre-release)";
        break;

      case update_status::check_failed:
        os << "failed to check for updates";
        break;
    }

    return os.str ();
  }
}
