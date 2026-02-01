#include <launcher/update/update-installer.hxx>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include <miniz.h>

using namespace std;

namespace launcher
{
  update_installer::
  update_installer (asio::io_context& c)
    : ioc_ (c),
      http_ (make_unique<http_client> (c))
  {
    // We default to the system temp directory for downloads. Note that if we
    // can't determine that (or don't have permissions), then we fall back to
    // the current working directory.
    //
    error_code ec;
    download_dir_ = fs::temp_directory_path (ec);
    if (ec)
      download_dir_ = fs::current_path ();
  }

  void update_installer::
  set_progress_callback (progress_callback_type cb)
  {
    progress_callback_ = move (cb);
  }

  void update_installer::
  set_download_directory (fs::path d)
  {
    download_dir_ = move (d);
  }

  void update_installer::
  set_verify_size (bool v)
  {
    verify_size_ = v;
  }

  asio::awaitable<update_result> update_installer::
  install (const update_info& ui)
  {
    update_result r;

    // Sanity check. If we don't have the basics, there is no point in
    // spinning up the pipeline.
    //
    if (ui.empty () || ui.asset_url.empty ())
    {
      r.error_message = "invalid update info";
      co_return r;
    }

    try
    {
      // We treat the update as a pipeline of artifacts: download -> extract
      // -> swap. We need to track the intermediate files (archive, binary) so
      // we can scrub them in cleanup () regardless of whether we succeed or
      // exception out.
      //

      // 1. Download.
      //
      fs::path a (co_await download_archive (ui));
      temp_files_.push_back (a);

      // If we have an expected size, verify it. This catches the nasty case
      // where a proxy or unstable connection gives us a truncated-but-valid
      // zip file that fails later during extraction (or worse, extracts
      // garbage).
      //
      if (verify_size_ && ui.asset_size > 0)
      {
        if (!validate_download (a, ui.asset_size))
        {
          r.error_message = "download validation failed: size mismatch";
          cleanup ();
          co_return r;
        }
      }

      // 2. Extract.
      //
      fs::path b (co_await extract_launcher (a, ui));
      temp_files_.push_back (b);

      // We expect the extractor to either throw or produce the file. If it
      // didn't throw but the file is missing, something is really wrong with
      // the environment.
      //
      if (!fs::exists (b))
      {
        r.error_message = "extraction failed: launcher binary not found";
        cleanup ();
        co_return r;
      }

      // 3. Swap.
      //
      // Identify our current location (t) and try to swap in the new binary
      // (b).
      //
      fs::path t (current_executable_path ());
      r = replace_launcher (b, t);

      if (!r.success)
      {
        // The swap failed. We attempt to rollback changes to restore the
        // original binary. If this also fails, we are likely leaving the user
        // with no launcher, but there isn't much else we can do at this
        // level.
        //
        rollback (r);
        cleanup ();
        co_return r;
      }

      cleanup ();
    }
    catch (const exception& e)
    {
      r.success = false;
      r.error_message = e.what ();
      cleanup ();
    }

    co_return r;
  }

bool update_installer::
  rollback (const update_result& r)
  {
    // If we didn't get far enough to define these paths, there is nothing
    // to rollback.
    //
    if (r.backup_path.empty () || r.installed_path.empty ())
      return false;

    error_code ec;

    // Remove the broken/partial installation if it exists.
    //
    if (fs::exists (r.installed_path))
    {
      fs::remove (r.installed_path, ec);
      if (ec)
        return false;
    }

    // Move the backup back to the original location. If this fails, the user
    // effectively has no launcher installed.
    //
    if (fs::exists (r.backup_path))
    {
      fs::rename (r.backup_path, r.installed_path, ec);
      return !ec;
    }

    return false;
  }

  void update_installer::
  cleanup ()
  {
    // Best-effort cleanup. If we fail to delete a temp file (e.g., AV lock),
    // we just ignore it.
    //
    error_code ec;
    for (const auto& p : temp_files_)
    {
      if (fs::exists (p))
      {
        if (fs::is_directory (p))
          fs::remove_all (p, ec);
        else
          fs::remove (p, ec);
      }
    }
    temp_files_.clear ();
  }

  bool update_installer::
  schedule_restart (const fs::path& n)
  {
    if (!fs::exists (n))
      return false;

#ifdef _WIN32
    // Windows is "special": we cannot overwrite the running executable
    // directly, nor can we easily exec() into a new process while
    // replacing the current one.
    //
    // Our workaround is a batch script trampoline:
    //
    // 1. Write a .bat that waits for us to die.
    // 2. Launch the .bat detached.
    // 3. Exit this process.
    // 4. .bat wakes up, launches the new binary, and deletes itself.
    //
    fs::path s (download_dir_ / "launcher_restart.bat");
    {
      ofstream os (s);
      os << "@echo off\n";
      os << "timeout /t 2 /nobreak > nul\n";
      os << "start \"\" \"" << n.string () << "\"\n";
      os << "del \"%~f0\"\n";
    }

    STARTUPINFOA si = {};
    si.cb = sizeof (si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    string cmd ("cmd.exe /c \"" + s.string () + "\"");

    if (CreateProcessA (nullptr,
                        cmd.data (),
                        nullptr,
                        nullptr,
                        FALSE,
                        CREATE_NO_WINDOW,
                        nullptr,
                        nullptr,
                        &si,
                        &pi))
    {
      // We don't need to track the child process.
      //
      CloseHandle (pi.hProcess);
      CloseHandle (pi.hThread);
      return true;
    }
    return false;

#else
    // On POSIX, things are civilized. We just mark the new binary executable
    // and replace the current process image.
    //
    error_code ec;
    fs::permissions (n,
                     fs::perms::owner_exec | fs::perms::group_exec |
                     fs::perms::others_exec,
                     fs::perm_options::add,
                     ec);
    if (ec)
      return false;

    // We assume the new binary takes no arguments for the restart.
    //
    execl (n.c_str (),
           n.filename ().c_str (),
           nullptr);

    // If we return, execl failed.
    //
    return false;
#endif
  }

  fs::path update_installer::
  current_executable_path ()
  {
    error_code ec;

#ifdef _WIN32
    char b[MAX_PATH];
    DWORD l (GetModuleFileNameA (nullptr, b, MAX_PATH));
    if (l > 0 && l < MAX_PATH)
      return fs::path (b);
#else
    // On Linux /proc/self/exe is the most reliable way to find where
    // we actually live.
    //
    fs::path l ("/proc/self/exe");
    fs::path r (fs::read_symlink (l, ec));
    if (!ec)
      return r;
#endif

    // Fallback: if the OS calls fail, we return the CWD. This is wrong
    // if the user ran us as `./bin/launcher` from root, but it's
    // better than throwing.
    //
    return fs::current_path (ec);
  }

  fs::path update_installer::
  backup_path (const fs::path& p)
  {
    return fs::path (p.string () + ".backup");
  }

  fs::path update_installer::
  staging_path (const fs::path& p)
  {
    return fs::path (p.string () + ".new");
  }

  asio::awaitable<fs::path> update_installer::
  download_archive (const update_info& ui)
  {
    fs::path t (download_dir_ / ui.asset_name);

    error_code ec;
    fs::create_directories (download_dir_, ec);

    // Translate the generic HTTP progress to our specific update state.
    //
    auto cb = [this, tot = ui.asset_size]
              (uint64_t cur, uint64_t /* hint */)
    {
      double p (tot > 0 ? static_cast<double> (cur) / tot : 0.0);
      report_progress (update_state::downloading, p, "Downloading...");
    };

    co_await http_->download (ui.asset_url,
                              t.string (),
                              cb,
                              nullopt,
                              0);

    co_return t;
  }

  asio::awaitable<fs::path> update_installer::
  extract_launcher (const fs::path& ap, const update_info& ui)
  {
    fs::path d (download_dir_ / "launcher_update_extract");
    error_code ec;
    fs::create_directories (d, ec);
    temp_files_.push_back (d);

    string e (ap.extension ().string ());
    transform (e.begin (),
               e.end (),
               e.begin (),
               [] (unsigned char c)
    {
      return static_cast<char> (tolower (c));
    });

    string fn (ap.filename ().string ());
    bool t (fn.size () > 7 && fn.substr (fn.size () - 7) == ".tar.xz");

    // @@: Again, we have a similar logic in manifest, we should make it
    // generic.
    //
    if (e == ".zip")
    {
      mz_zip_archive z = {};
      if (!mz_zip_reader_init_file (&z, ap.string ().c_str (), 0))
        throw runtime_error ("failed to open zip archive");

      mz_uint n (mz_zip_reader_get_num_files (&z));

      for (mz_uint i (0); i < n; ++i)
      {
        mz_zip_archive_file_stat s;
        if (!mz_zip_reader_file_stat (&z, i, &s))
          continue;

        if (mz_zip_reader_is_file_a_directory (&z, i))
          continue;

        fs::path p (d / s.m_filename);
        fs::create_directories (p.parent_path (), ec);

        if (!mz_zip_reader_extract_to_file (&z, i,
                                            p.string ().c_str (), 0))
        {
          mz_zip_reader_end (&z);
          throw runtime_error ("failed to extract: " + string (s.m_filename));
        }
      }

      mz_zip_reader_end (&z);
    }
    else if (t)
    {
      string c ("tar -xJf \"" + ap.string () + "\" -C \"" +
                d.string () + "\"");

      int r (system (c.c_str ()));
      if (r != 0)
        throw runtime_error ("failed to extract tar.xz archive");
    }
    else
      throw runtime_error ("unsupported archive format: " + e);

    // We don't make assumption about the exact internal structure of the
    // archive (e.g., if there is a top-level directory or not), so we
    // recursively scan for anything that looks like our binary.
    //
    vector<string> bns = {
      "launcher",
      "iw4x-launcher",
      "launcher.exe",
      "iw4x-launcher.exe"
    };

    for (auto& en : fs::recursive_directory_iterator (d))
    {
      if (!en.is_regular_file ())
        continue;

      string n (en.path ().filename ().string ());
      for (const auto& bn : bns)
      {
        if (n == bn)
          co_return en.path ();
      }
    }

    throw runtime_error ("launcher binary not found in archive");
  }

  bool update_installer::
  validate_download (const fs::path& p, uint64_t s)
  {
    error_code ec;
    auto as (fs::file_size (p, ec));

    if (ec)
      return false;

    return as == s;
  }

  update_result update_installer::
  replace_launcher (const fs::path& nb, const fs::path& t)
  {
    update_result r;
    r.installed_path = t;

    error_code ec;
    fs::path b (backup_path (t));
    fs::path s (staging_path (t));

    // Copy the new binary to a .new file alongside the target.
    //
    fs::copy_file (nb, s,
                   fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
      r.error_message = "failed to copy new binary: " + ec.message ();
      return r;
    }

#ifndef _WIN32
    fs::permissions (s,
                     fs::perms::owner_exec | fs::perms::group_exec |
                     fs::perms::others_exec,
                     fs::perm_options::add,
                     ec);
#endif

    // Move the current executable to .backup.
    //
    if (fs::exists (t))
    {
      if (fs::exists (b))
        fs::remove (b, ec);

      fs::rename (t, b, ec);
      if (ec)
      {
        // On Windows, the running executable is locked. We can't rename it,
        // but sometimes we can move it or copy it. If rename fails, we try
        // copy.
        //
        fs::copy_file (t, b, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
          r.error_message = "failed to backup current launcher: " +
                            ec.message ();
          fs::remove (s, ec);
          return r;
        }
      }
      r.backup_path = b;
    }

    // Rename .new to target. This is atomic on POSIX, but not on Windows if
    // the target exists (which it shouldn't, we just moved it).
    //
    fs::rename (s, t, ec);
    if (ec)
    {
      // If the rename failed, try the rougher approach: copy over and
      // delete source.
      //
      fs::copy_file (s, t, fs::copy_options::overwrite_existing, ec);
      if (ec)
      {
        r.error_message = "failed to install new launcher: " +
                           ec.message ();

        // Panic mode: try to put the backup back.
        //
        if (!r.backup_path.empty () && fs::exists (b))
          fs::rename (b, t, ec);
        return r;
      }
      fs::remove (s, ec);
    }

    r.success = true;
    return r;
  }

  void update_installer::
  report_progress (update_state s, double p, const string& m)
  {
    if (progress_callback_)
      progress_callback_ (s, p, m);
  }
}
