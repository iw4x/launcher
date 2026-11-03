// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/process.hxx>

#include <cerrno>
#include <cstdlib>  // getenv()
#include <string_view>

// Note that Boost.Process includes <winsock2.h>, which has to come before
// <windows.h>.
//
#ifdef _WIN32
#  include <boost/asio/io_context.hpp>

#  include <boost/process/v2/stdio.hpp>
#  include <boost/process/v2/process.hpp>
#  include <boost/process/v2/environment.hpp>
#  include <boost/process/v2/windows/creation_flags.hpp>

#  include <windows.h>
#else
#  include <fcntl.h>     // open(), O_*
#  include <signal.h>
#  include <unistd.h>
#  include <sys/stat.h>
#  include <sys/wait.h>
#endif

#include <launcher/utility.hxx> // from_utf8()

using namespace std;

namespace launcher
{
#ifndef _WIN32
  namespace
  {
    // File descriptor that we own and close on destruction.
    //
    class auto_fd
    {
    public:
      explicit
      auto_fd (int fd = -1) noexcept: fd_ (fd) {}

      auto_fd (auto_fd&& x) noexcept: fd_ (x.release ()) {}
      auto_fd&
      operator= (auto_fd&& x) noexcept
      {
        reset (x.release ());
        return *this;
      }

      auto_fd (const auto_fd&) = delete;
      auto_fd& operator= (const auto_fd&) = delete;

      ~auto_fd () {reset ();}

      int
      get () const noexcept {return fd_;}

      int
      release () noexcept {int r (fd_); fd_ = -1; return r;}

      void
      reset (int fd = -1) noexcept
      {
        if (fd_ != -1)
          close (fd_);

        fd_ = fd;
      }

    private:
      int fd_;
    };

    // Return the program to execute, that is, the program itself if it
    // contains a directory separator and the first executable regular file
    // with its name in the PATH directories otherwise. Throw process_error
    // with ENOENT if there is none.
    //
    // Note that we skip the empty and relative PATH entries, which would
    // otherwise run whatever happens to be in the current directory.
    //
    string
    path_search (const string& p)
    {
      if (p.find ('/') != string::npos)
        return p;

      if (const char* ps = getenv ("PATH"))
      {
        for (string_view s (ps); !s.empty (); )
        {
          size_t n (s.find (':'));
          string_view d (s.substr (0, n));
          s = n != string_view::npos ? s.substr (n + 1) : string_view ();

          if (d.empty () || d.front () != '/')
            continue;

          string r (d);
          r += '/';
          r += p;

          struct stat st;
          if (stat (r.c_str (), &st) == 0 &&
              S_ISREG (st.st_mode)         &&
              access (r.c_str (), X_OK) == 0)
            return r;
        }
      }

      throw process_error (ENOENT, "unable to execute {}", p);
    }

    // Report the error to the parent through the descriptor and exit. Only
    // for the child processes.
    //
    [[noreturn]] void
    fail_child (int fd, int e) noexcept
    {
      // Note that a write of this size to a pipe is atomic and there is
      // nothing we could do about a failure anyway.
      //
      [[maybe_unused]] ssize_t r (write (fd, &e, sizeof (e)));
      _exit (1);
    }
  }
#endif

  void
  start_detached (const string& p, const vector<string>& as)
  {
#ifdef _WIN32
    namespace bp = boost::process::v2;

    // Note that once we detach from the process (that is, close its
    // handles) it leaves nothing behind when it exits.
    //
    try
    {
      // CreateProcessW() only searches PATH for the program in the command
      // line, which Boost.Process doesn't use to find it.
      //
      bp::filesystem::path f (from_utf8 (p).native ());

      if (p.find_first_of ("/\\") == string::npos)
      {
        f = bp::environment::find_executable (f);

        if (f.empty ())
          throw process_error (error_code (ERROR_FILE_NOT_FOUND,
                                           system_category ()),
                               "unable to execute {}",
                               p);
      }

      boost::asio::io_context ctx;
      bp::process c (
        ctx,
        f,
        as,
        bp::process_stdio {nullptr, nullptr, nullptr},
        bp::windows::process_creation_flags<DETACHED_PROCESS |
                                            CREATE_NEW_PROCESS_GROUP> ());

      c.detach ();
    }
    catch (const process_error&)
    {
      throw;
    }
    catch (const system_error& e)
    {
      throw process_error (e.code (), "unable to execute {}", p);
    }
#else
    // Prepare everything that the children need. After fork() in a
    // multi-threaded process they may only call async-signal-safe functions,
    // which, in particular, rules out malloc() and thus also the PATH search
    // in execvp().
    //
    string f (path_search (p));

    vector<char*> av;
    av.push_back (const_cast<char*> (p.c_str ()));

    for (const string& a : as)
      av.push_back (const_cast<char*> (a.c_str ()));

    av.push_back (nullptr);

    // The children report their failures through the pipe as errno. A
    // successful exec() closes the grandchild's end and our child's end
    // goes away when it exits, so if we read nothing, then the program has
    // started.
    //
    auto_fd in;
    auto_fd out;
    {
      int fd[2];
      if (pipe2 (fd, O_CLOEXEC) == -1)
        throw process_error (errno, "unable to execute {}", p);

      in.reset (fd[0]);
      out.reset (fd[1]);
    }

    // Block all the signals for the duration of fork() so that our handlers
    // don't run in the child before it resets them.
    //
    sigset_t pm; // Previous mask.
    {
      sigset_t m;
      sigfillset (&m);

      if (int e = pthread_sigmask (SIG_SETMASK, &m, &pm))
        throw process_error (e, "unable to execute {}", p);
    }

    pid_t c (fork ());
    int e (errno);

    if (c == 0)
    {
      // Child.
      //
      // Reset all the signals to their default handlers and unblock them.
      // There is no portable way to iterate over the valid signal numbers,
      // so we iterate over [1, NSIG) and ignore the sigaction() errors for
      // the numbers that are not signals.
      //
      {
        struct sigaction sa {};
        sigemptyset (&sa.sa_mask);
        sa.sa_handler = SIG_DFL;

        for (int s (1); s != NSIG; ++s)
        {
          if (s != SIGKILL && s != SIGSTOP)
            sigaction (s, &sa, nullptr);
        }

        sigset_t m;
        sigfillset (&m);

        if (int r = pthread_sigmask (SIG_UNBLOCK, &m, nullptr))
          fail_child (out.get (), r);
      }

      // Start a new session, which detaches the program from our
      // controlling terminal and from the signals that the terminal sends
      // to our process group. Note that this cannot fail since a forked
      // child is never a process group leader.
      //
      setsid ();

      // Fork the grandchild, which runs the program, and exit so that init
      // adopts it.
      //
      pid_t g (fork ());

      if (g == -1)
        fail_child (out.get (), errno);

      if (g != 0)
        _exit (0);

      // Grandchild.
      //
      // Connect the standard streams to the null device.
      //
      {
        int n (open ("/dev/null", O_RDWR));

        if (n == -1)
          fail_child (out.get (), errno);

        if (dup2 (n, STDIN_FILENO)  == -1 ||
            dup2 (n, STDOUT_FILENO) == -1 ||
            dup2 (n, STDERR_FILENO) == -1)
          fail_child (out.get (), errno);

        if (n > STDERR_FILENO)
          close (n);
      }

      execv (f.c_str (), av.data ());
      fail_child (out.get (), errno);
    }

    // Parent.
    //
    // Restore the signal mask. This shouldn't fail unless something is
    // severely damaged, and the fork() result is more interesting anyway.
    //
    pthread_sigmask (SIG_SETMASK, &pm, nullptr);

    if (c == -1)
      throw process_error (e, "unable to execute {}", p);

    out.reset ();

    // Wait for our child, which exits as soon as it has forked the
    // grandchild, so that it doesn't remain our zombie.
    //
    int s;
    while (waitpid (c, &s, 0) == -1)
    {
      if (errno != EINTR)
        throw process_error (errno, "unable to wait for {}", p);
    }

    // See if the children failed. They report every failure through the
    // pipe, so a child that didn't exit normally must have been killed,
    // presumably by a signal sent to it from outside.
    //
    ssize_t n;
    while ((n = read (in.get (), &e, sizeof (e))) == -1 && errno == EINTR) ;

    if (n == sizeof (e))
      throw process_error (e, "unable to execute {}", p);

    if (!WIFEXITED (s) || WEXITSTATUS (s) != 0)
      throw process_error (EINTR, "unable to execute {}", p);
#endif
  }
}
