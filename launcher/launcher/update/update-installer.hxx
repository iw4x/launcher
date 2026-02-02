#pragma once

#include <boost/asio.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <launcher/download/download.hxx>
#include <launcher/http/http.hxx>
#include <launcher/update/update-types.hxx>

namespace launcher
{
  namespace asio = boost::asio;
  namespace fs = std::filesystem;

  struct update_result
  {
    bool success = false;
    std::string error_message;
    fs::path installed_path;
    fs::path backup_path;

    explicit operator bool () const noexcept { return success; }
  };

  // We need to handle the update process carefully to avoid leaving the user
  // with a broken (partial) installation. The general strategy is
  // side-by-side installation: we download and extract to a temp spot, and
  // only touch the real installation once we have verified the bits.
  //
  // Windows makes this tricky because we can't overwrite a running
  // executable. So the dance is: rename current -> backup, move new ->
  // current.
  //
  class update_installer
  {
  public:
    using progress_callback_type =
      std::function<void (update_state state,
                          double progress,
                          const std::string& message)>;

    // We keep the context to schedule our async work and the http client
    // because we need to reuse connections and settings across the download
    // phase.
    //
    explicit
    update_installer (asio::io_context& ioc);

    update_installer (const update_installer&) = delete;
    update_installer& operator= (const update_installer&) = delete;

    // Configuration.
    //

    // Set progress callback for status updates.
    //
    void
    set_progress_callback (progress_callback_type callback);

    // By default we use system temp. If the user wants us to keep the dirty
    // laundry somewhere else, they can override it here.
    //
    void
    set_download_directory (fs::path dir);

    // If true, we double-check the content length. Usually a good idea
    // unless the server is misbehaving or we are testing locally.
    //
    void
    set_verify_size (bool verify);

    // Mechanics.
    //

    // The main driver. We assume 'i' contains a valid URL. We don't catch
    // exceptions here. That is, we let them propagate up so the caller can
    // decide if they want to retry or abort.
    //
    // Note that we return a result struct rather than throwing on logic
    // errors (like hash mismatch) because those are expected runtime outcomes
    // we want to display.
    //
    asio::awaitable<update_result>
    install (const update_info& info);

    // Try to undo the damage. This is a best-effort attempt to move the
    // backup file back to the original location.
    //
    bool
    rollback (const update_result& result);

    // We try to clean up the staging area. We are doing this manually to
    // inspect the debris if the install fails.
    //
    void
    cleanup ();

    // Restart logic
    //

    // Scheduling a restart is OS-specific hell. On POSIX we could just
    // exec(), but on Windows we need to spawn a batch script to handle the
    // delay while this process dies.
    //
    bool
    schedule_restart (const fs::path& new_launcher_path);

    // Path.
    //

    // Get the path to the currently running launcher executable.
    //
    static fs::path
    current_executable_path ();

    // Get the backup path for the current executable.
    //
    static fs::path
    backup_path (const fs::path& original);

    // Get the temporary path for the new executable during installation.
    //
    static fs::path
    staging_path (const fs::path& original);

  private:
    // Just fetch the bytes. We stream this to disk to keep memory usage low,
    // as the update bundle might be hefty.
    //
    asio::awaitable<fs::path>
    download_archive (const update_info& info);

    // Extract the launcher binary from the archive.
    //
    asio::awaitable<fs::path>
    extract_launcher (const fs::path& archive_path,
                      const update_info& info);

    // Perform safe replacement of the launcher binary.
    //
    // Uses a two-step process:
    // 1. Rename current to backup
    // 2. Rename new to current
    //
    // If step 2 fails, restores from backup.
    //
    update_result
    replace_launcher (const fs::path& new_binary,
                      const fs::path& target);

    // Report progress.
    //
    void
    report_progress (update_state state,
                     double progress,
                     const std::string& message);

    asio::io_context& ioc_;
    std::unique_ptr<http_client> http_;
    progress_callback_type progress_callback_;
    fs::path download_dir_;
    bool verify_size_ = true;
    std::vector<fs::path> temp_files_;
  };
}
