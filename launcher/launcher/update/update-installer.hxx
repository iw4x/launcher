// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <boost/asio.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <launcher/download/download.hxx>
#include <launcher/http/http.hxx>
#include <launcher/update/update-types.hxx>

namespace launcher
{
  namespace asio = boost::asio;
  namespace fs = std::filesystem;

  // Outcome of update_installer::install(). The paths are those of the
  // replaced executable and its backup, as far as we got.
  //
  struct update_result
  {
    bool success = false;
    std::string error_message; // If not successful.
    fs::path installed_path;
    fs::path backup_path;

    explicit operator bool () const noexcept { return success; }
  };

  // Launcher self-update from a release asset.
  //
  // We need to be careful not to leave the user with a broken launcher. So
  // we download and extract in a temporary location and only touch the
  // installed executable once we have the new one in hand.
  //
  // Windows makes this tricky because we can't overwrite a running
  // executable. So the dance is: rename current -> backup, rename new ->
  // current.
  //
  class update_installer
  {
  public:
    using progress_callback_type =
      std::function<void (update_state state,
                          double progress,
                          const std::string& message)>;

    explicit
    update_installer (asio::io_context& ioc);

    update_installer (const update_installer&) = delete;
    update_installer& operator= (const update_installer&) = delete;

    // Configuration.
    //

    void
    set_progress_callback (progress_callback_type callback);

    // Download and extract in dir. By default it is the system temporary
    // directory or, if we cannot determine it, the current directory.
    //
    void
    set_download_directory (fs::path dir);

    // @@ Nothing reads this setting.
    //
    void
    set_verify_size (bool verify);

    // Mechanics.
    //

    // Replace the running launcher executable with the one in the release
    // asset, keeping a backup. Report failures in the result, which is what
    // the caller displays.
    //
    asio::awaitable<update_result>
    install (const update_info& info);

    // Move the backup back over the installed executable. Return false if
    // there is no backup or we could not restore it.
    //
    bool
    rollback (const update_result& result);

    // Remove the downloaded archive and the extracted files, ignoring
    // errors.
    //
    void
    cleanup ();

    // Restart.
    //

    // Start the new launcher with the arguments, normally the ones that we
    // were started with, so that options such as --arch survive the update.
    // Return false on failure.
    //
    // On POSIX we replace this process (exec), so we only return on failure.
    // On Windows we start a batch script that gives us 2 seconds to exit
    // before starting the new launcher, so the caller should exit.
    //
    bool
    schedule_restart (const fs::path& new_launcher_path,
                      const std::vector<std::string>& args);

    // Path.
    //

    // Return the path of the running launcher executable.
    //
    static fs::path
    current_executable_path ();

    // Return the backup path for the executable (<original>.backup).
    //
    static fs::path
    backup_path (const fs::path& original);

    // Return the path that the new executable is copied to before it
    // replaces the original (<original>.new).
    //
    static fs::path
    staging_path (const fs::path& original);

  private:
    // Download the asset into the download directory and return its path.
    // We stream it to disk since the archive can be hefty.
    //
    asio::awaitable<fs::path>
    download_archive (const update_info& info);

    // Extract the archive and return the path of the launcher executable in
    // it. We use the tar program for .tar.xz.
    //
    asio::awaitable<fs::path>
    extract_launcher (const fs::path& archive_path,
                      const update_info& info);

    // Replace the target with the new executable, keeping the old one as the
    // backup. We first copy the new one next to the target (see
    // staging_path()), so that the replacement is a rename within one
    // directory. If installing fails, then restore the backup.
    //
    update_result
    replace_launcher (const fs::path& new_binary,
                      const fs::path& target);

    void
    report_progress (update_state state,
                     double progress,
                     const std::string& message);

    asio::io_context& ioc_;
    http_client http_;
    progress_callback_type progress_callback_;
    fs::path download_dir_;
    bool verify_size_ = true;
    std::vector<fs::path> temp_files_;
  };
}
