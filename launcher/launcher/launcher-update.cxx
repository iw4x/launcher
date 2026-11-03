// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/launcher-update.hxx>

#include <atomic>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <launcher/version.hxx>

using namespace std;

namespace launcher
{
  update_coordinator::
  update_coordinator (asio::io_context& c)
    : discovery_ (c),
      installer_ (c)
  {
    installer_.set_progress_callback (
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
    discovery_.set_token (move (t));
  }

  void update_coordinator::
  set_include_prerelease (bool i)
  {
    discovery_.set_include_prerelease (i);
  }

  void update_coordinator::
  set_progress_callback (progress_callback_type c)
  {
    progress_callback_ = move (c);
  }

  void update_coordinator::
  set_completion_callback (completion_callback_type c)
  {
    completion_callback_ = move (c);
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
    state_ = update_state::checking;

    try
    {
      last_update_info_ = co_await discovery_.check_for_update (
        owner_, repo_, current_version_);

      if (last_update_info_.empty ())
      {
        state_ = update_state::idle;

        if (headless_)
          cout << "launcher is up to date (" << current_version_ << ")." << endl;

        report_completion (update_status::up_to_date);
        co_return update_status::up_to_date;
      }

      state_ = update_state::idle;
      report_completion (update_status::update_available);
      co_return update_status::update_available;
    }
    catch (const exception& e)
    {
      state_ = update_state::failed;
      report_completion (update_status::check_failed, e.what ());
      co_return update_status::check_failed;
    }
  }

  asio::awaitable<update_result> update_coordinator::
  install_update (info_type i)
  {
    if (i.empty () || i.asset_url.empty ())
    {
      update_result r;
      r.error_message = "invalid update info";
      co_return r;
    }

    // Show the download as a progress entry. Note that the installer reports
    // the ratio, which we turn into bytes with the asset size.
    //
    shared_ptr<progress_entry> e;

    if (progress_coord_ != nullptr)
    {
      e = progress_coord_->add_entry (i.asset_name);
      e->metrics ().total_bytes.store (i.asset_size, memory_order_relaxed);

      installer_.set_progress_callback (
        [this, e] (update_state s, double p, const string& m)
      {
        if (s == update_state::downloading && e)
        {
          uint64_t c (static_cast<uint64_t> (
            p * e->metrics ().total_bytes.load (memory_order_relaxed)));

          progress_coord_->update_progress (
            e,
            c,
            e->metrics ().total_bytes.load (memory_order_relaxed));
        }

        report_progress (s, p, m);
      });
    }

    auto r (co_await installer_.install (i));

    if (e && progress_coord_ != nullptr)
      progress_coord_->remove_entry (e);

    if (r.success)
    {
      state_ = update_state::completed;
      last_installed_path_ = r.installed_path;
    }
    else
      state_ = update_state::failed;

    co_return r;
  }

  bool update_coordinator::
  restart (const vector<string>& args)
  {
    // The installer moves the running executable to its .backup, so we
    // restart through the path that it installed the update at.
    //
    fs::path t;
    if (!last_installed_path_.empty () && fs::exists (last_installed_path_))
      t = last_installed_path_;
    else
      t = installer_type::current_executable_path ();

    state_ = update_state::restarting;
    report_progress (update_state::restarting, 0.0, "Restarting...");

    return installer_.schedule_restart (t, args);
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
    return discovery_;
  }

  const update_coordinator::discovery_type& update_coordinator::
  discovery () const noexcept
  {
    return discovery_;
  }

  update_coordinator::installer_type& update_coordinator::
  installer () noexcept
  {
    return installer_;
  }

  const update_coordinator::installer_type& update_coordinator::
  installer () const noexcept
  {
    return installer_;
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
    auto uc (make_unique<update_coordinator> (c));

    launcher_version v;
    v.major = LAUNCHER_VERSION_MAJOR;
    v.minor = LAUNCHER_VERSION_MINOR;
    v.patch = LAUNCHER_VERSION_PATCH;

    // The pre-release and snapshot parts only come with the version string.
    //
    auto p (parse_launcher_version (LAUNCHER_VERSION_STR));
    if (p)
    {
      v.pre_release = p->pre_release;
      v.snapshot_sn = p->snapshot_sn;
      v.snapshot_id = p->snapshot_id;
    }

    uc->set_current_version (v);
    return uc;
  }

  string
  format_update_status (update_status s, const update_info& i)
  {
    ostringstream os;

    switch (s)
    {
    case update_status::up_to_date:
      {
        os << "launcher is up to date";
        if (!i.empty ())
          os << " (" << i.version << ")";
        break;
      }
    case update_status::update_available:
      {
        os << "update available: " << i.version;
        if (i.prerelease)
          os << " (pre-release)";
        break;
      }
    case update_status::check_failed:
      {
        os << "failed to check for updates";
        break;
      }
    }

    return os.str ();
  }
}
