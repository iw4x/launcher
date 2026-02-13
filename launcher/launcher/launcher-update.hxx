#pragma once

#include <boost/asio.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include <launcher/launcher-progress.hxx>
#include <launcher/progress/progress.hxx>
#include <launcher/update/update.hxx>

namespace launcher
{
  namespace asio = boost::asio;
  namespace fs = std::filesystem;

  class update_coordinator
  {
  public:
    using discovery_type = update_discovery;
    using installer_type = update_installer;
    using version_type = launcher_version;
    using info_type = update_info;

    // Progress callback.
    //
    // Called during update with current state and progress.
    //
    using progress_callback_type =
      std::function<void (update_state state,
                          double progress,
                          const std::string& message)>;

    // Completion callback.
    //
    // Called when update check or installation completes. The info parameter
    // contains the update information (empty if up-to-date or failed).
    //
    using completion_callback_type =
      std::function<void (update_status status,
                          const info_type& info,
                          const std::string& error)>;

    // Constructor.
    //
    explicit
    update_coordinator (asio::io_context& ioc);

    update_coordinator (const update_coordinator&) = delete;
    update_coordinator& operator= (const update_coordinator&) = delete;

    // Configuration.
    //

    // Set the GitHub repository to check for updates.
    //
    // Default: "iw4x" / "launcher"
    //
    void
    set_repository (std::string owner, std::string repo);

    // Set the current launcher version for comparison.
    //
    void
    set_current_version (const version_type& version);

    // Set the current launcher version from version string. Throws if parsing
    // fails.
    //
    void
    set_current_version (const std::string& version_str);

    // Set GitHub authentication token (optional, for higher rate limits).
    //
    void
    set_token (std::string token);

    // Set whether to include pre-releases in update checks.
    //
    void
    set_include_prerelease (bool include);

    // Set progress callback.
    //
    void
    set_progress_callback (progress_callback_type callback);

    // Set completion callback.
    //
    void
    set_completion_callback (completion_callback_type callback);

    // Set whether to automatically restart after update.
    //
    void
    set_auto_restart (bool restart);

    // Set whether to run in headless mode (no prompts).
    //
    void
    set_headless (bool headless);

    // Set progress coordinator for TUI progress display.
    //
    // When set, download progress is displayed via the progress_coordinator
    // instead of simple console output.
    //
    void
    set_progress_coordinator (progress_coordinator* progress);

    // Operations.
    //

    // Check for updates without installing.
    //
    // Returns update_info if an update is available, empty info if up-to-date.
    // Throws on network errors.
    //
    asio::awaitable<update_status>
    check_for_updates ();

    // Install a specific update (from update_info).
    //
    asio::awaitable<update_result>
    install_update (const info_type& info);

    // Restart the launcher to run the new version.
    //
    bool
    restart ();

    // Accessors.
    //

    // Get the current update state.
    //
    update_state
    state () const noexcept;

    // Get the last update info (from most recent check).
    //
    const info_type&
    last_update_info () const noexcept;

    // Get the current launcher version.
    //
    const version_type&
    current_version () const noexcept;

    // Check if an update is available.
    //
    bool
    update_available () const noexcept;

    // Access underlying components.
    //
    discovery_type&
    discovery () noexcept;

    const discovery_type&
    discovery () const noexcept;

    installer_type&
    installer () noexcept;

    const installer_type&
    installer () const noexcept;

  private:
    // Report progress.
    //
    void
    report_progress (update_state state,
                     double progress,
                     const std::string& message);

    // Report completion.
    //
    void
    report_completion (update_status status,
                       const std::string& error = "");

    asio::io_context& ioc_;
    std::unique_ptr<discovery_type> discovery_;
    std::unique_ptr<installer_type> installer_;

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

  // Create an update coordinator from the current launcher version.
  //
  // Uses the compiled-in version constants to set the current version.
  //
  std::unique_ptr<update_coordinator>
  make_update_coordinator (asio::io_context& ioc);

  // Format update status for display.
  //
  std::string
  format_update_status (update_status status, const update_info& info);
}
