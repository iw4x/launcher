// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <boost/asio.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <launcher/launcher-progress.hxx>
#include <launcher/progress/progress.hxx>
#include <launcher/update/update.hxx>

namespace launcher
{
  namespace asio = boost::asio;
  namespace fs = std::filesystem;

  // Update of the launcher itself from its GitHub releases.
  //
  class update_coordinator
  {
  public:
    using discovery_type = update_discovery;
    using installer_type = update_installer;
    using version_type = launcher_version;
    using info_type = update_info;

    // Called as the update progresses, with the state, the progress ratio,
    // and the message to show.
    //
    using progress_callback_type =
      std::function<void (update_state state,
                          double progress,
                          const std::string& message)>;

    // Called once the check completes, with the update that it found, which
    // is empty if there is none, and the error if the check failed.
    //
    using completion_callback_type =
      std::function<void (update_status status,
                          const info_type& info,
                          const std::string& error)>;

    explicit
    update_coordinator (asio::io_context& ioc);

    update_coordinator (const update_coordinator&) = delete;
    update_coordinator& operator= (const update_coordinator&) = delete;

    // Configuration.
    //

    // Set the GitHub repository of the releases, iw4x/launcher by default.
    //
    void
    set_repository (std::string owner, std::string repo);

    // Set the version of the running launcher, which a release has to be
    // newer than to be an update.
    //
    void
    set_current_version (const version_type& version);

    // As above but parse the version. Throw std::invalid_argument if it is
    // invalid.
    //
    void
    set_current_version (const std::string& version_str);

    // Authenticate the GitHub requests with the token, which raises the rate
    // limit.
    //
    void
    set_token (std::string token);

    void
    set_include_prerelease (bool include);

    void
    set_progress_callback (progress_callback_type callback);

    void
    set_completion_callback (completion_callback_type callback);

    // @@ Nothing reads this setting.
    //
    void
    set_auto_restart (bool restart);

    // In the headless mode the check also prints its result to stdout.
    //
    void
    set_headless (bool headless);

    // Set the coordinator that shows the progress of the download.
    //
    void
    set_progress_coordinator (progress_coordinator* progress);

    // Updating.
    //

    // Check for a release that is newer than the running launcher and
    // return the outcome, which also goes to the completion callback. The
    // update that the check finds is available as last_update_info().
    //
    asio::awaitable<update_status>
    check_for_updates ();

    // Download the update and put it in place of the running launcher.
    //
    asio::awaitable<update_result>
    install_update (const info_type& info);

    // Restart into the installed update, or into the running executable if
    // there is none, passing it the arguments (see
    // update_installer::schedule_restart()).
    //
    bool
    restart (const std::vector<std::string>& args);

    update_state
    state () const noexcept;

    const info_type&
    last_update_info () const noexcept;

    const version_type&
    current_version () const noexcept;

    // Return true if the last check found a newer release.
    //
    bool
    update_available () const noexcept;

    discovery_type&
    discovery () noexcept;

    const discovery_type&
    discovery () const noexcept;

    installer_type&
    installer () noexcept;

    const installer_type&
    installer () const noexcept;

  private:
    void
    report_progress (update_state state,
                     double progress,
                     const std::string& message);

    void
    report_completion (update_status status,
                       const std::string& error = "");

    discovery_type discovery_;
    installer_type installer_;

    std::string owner_ = "iw4x";
    std::string repo_ = "launcher";
    version_type current_version_;
    info_type last_update_info_;
    update_state state_ = update_state::idle;

    progress_callback_type progress_callback_;
    completion_callback_type completion_callback_;
    progress_coordinator* progress_coord_ = nullptr;
    fs::path last_installed_path_;
    bool auto_restart_ = false;
    bool headless_ = false;
  };

  // Create the update coordinator with the version of this launcher.
  //
  std::unique_ptr<update_coordinator>
  make_update_coordinator (asio::io_context& ioc);

  // Return the status of the check as a message.
  //
  std::string
  format_update_status (update_status status, const update_info& info);
}
