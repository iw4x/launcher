#include <launcher/steam/steam-prompt.hxx>

#include <boost/asio/use_awaitable.hpp>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <io.h>
#  include <windows.h>
#else
#  include <termios.h>
#  include <unistd.h>
#endif

extern "C"
{
#include <launcher/qrcodegen/qrcodegen.h>
}

#include <launcher/launcher-log.hxx>

using namespace std;

namespace launcher
{
  namespace prompt
  {
    namespace
    {
      constexpr auto info ([] (auto&&... a)
      {
        log::info (categories::steam (), forward<decltype (a)> (a)...);
      });

      constexpr auto warning ([] (auto&&... a)
      {
        log::warning (categories::steam (), forward<decltype (a)> (a)...);
      });

      bool
      use_colour () noexcept
      {
        if (const char* v = getenv ("NO_COLOR"); v != nullptr && *v != '\0')
          return false;

        return interactive ();
      }

      class echo_guard
      {
      public:
        echo_guard ()
        {
#ifdef _WIN32
          handle_ = GetStdHandle (STD_INPUT_HANDLE);

          if (handle_ == INVALID_HANDLE_VALUE || handle_ == nullptr)
            return;

          if (!GetConsoleMode (handle_, &previous_))
          {
            handle_ = INVALID_HANDLE_VALUE;
            return;
          }

          suppressed_ = SetConsoleMode (handle_,
                                        previous_ & ~ENABLE_ECHO_INPUT) != 0;
#else
          if (tcgetattr (STDIN_FILENO, &previous_) != 0)
            return;

          termios t (previous_);
          t.c_lflag &= static_cast<tcflag_t> (~ECHO);

          suppressed_ = tcsetattr (STDIN_FILENO, TCSAFLUSH, &t) == 0;
#endif
        }

        ~echo_guard ()
        {
          if (!suppressed_)
            return;

#ifdef _WIN32
          SetConsoleMode (handle_, previous_);
#else
          tcsetattr (STDIN_FILENO, TCSAFLUSH, &previous_);
#endif
        }

        echo_guard (const echo_guard&) = delete;
        echo_guard& operator= (const echo_guard&) = delete;

        bool
        suppressed () const noexcept
        {
          return suppressed_;
        }

      private:
        bool suppressed_ = false;

#ifdef _WIN32
        HANDLE handle_ = INVALID_HANDLE_VALUE;
        DWORD  previous_ = 0;
#else
        termios previous_ {};
#endif
      };

      string
      trim (string s)
      {
        size_t b (s.find_first_not_of (" \t\r\n"));

        if (b == string::npos)
          return string ();

        size_t e (s.find_last_not_of (" \t\r\n"));

        return s.substr (b, e - b + 1);
      }
    }

    bool
    interactive () noexcept
    {
#ifdef _WIN32
      return _isatty (_fileno (stdin)) != 0 && _isatty (_fileno (stdout)) != 0;
#else
      return isatty (STDIN_FILENO) != 0 && isatty (STDOUT_FILENO) != 0;
#endif
    }

    string
    read_line (const string& m)
    {
      cout << m << flush;

      string s;

      if (!getline (cin, s))
      {
        cout << "\n";
        return string ();
      }

      return trim (move (s));
    }

    string
    read_secret (const string& m)
    {
      echo_guard g;

      if (!g.suppressed ())
        warning ("this terminal will not let the launcher hide typed input; "
                 "what you type next will be visible on screen");

      cout << m << flush;

      string s;
      bool   ok (static_cast<bool> (getline (cin, s)));

      cout << "\n" << flush;

      if (!ok)
        return string ();

      return trim (move (s));
    }

    void
    render_qr (const string& t)
    {
      vector<uint8_t> qr (qrcodegen_BUFFER_LEN_MAX);
      vector<uint8_t> tmp (qrcodegen_BUFFER_LEN_MAX);

      if (!qrcodegen_encodeText (t.c_str (),
                                 tmp.data (),
                                 qr.data (),
                                 qrcodegen_Ecc_MEDIUM,
                                 qrcodegen_VERSION_MIN,
                                 qrcodegen_VERSION_MAX,
                                 qrcodegen_Mask_AUTO,
                                 true))
      {
        warning ("unable to render the login QR code; open the URL below "
                 "instead");

        cout << "\n  " << t << "\n\n" << flush;
        return;
      }

      int n (qrcodegen_getSize (qr.data ()));

      constexpr int quiet (4);

      const bool colour (use_colour ());

      const char* open  (colour ? "\033[47;30m" : "");
      const char* close (colour ? "\033[0m" : "");

      auto dark ([&qr, n] (int x, int y)
      {
        if (x < 0 || y < 0 || x >= n || y >= n)
          return false;

        return qrcodegen_getModule (qr.data (), x, y);
      });

      cout << "\n";

      for (int y (-quiet); y < n + quiet; y += 2)
      {
        cout << open;

        for (int x (-quiet); x < n + quiet; ++x)
        {
          bool top (dark (x, y));
          bool bot (dark (x, y + 1));

          if (top && bot)
            cout << "█";
          else if (top)
            cout << "▀";
          else if (bot)
            cout << "▄";
          else
            cout << ' ';
        }

        cout << close << "\n";
      }

      cout << "\n  " << t << "\n\n" << flush;
    }

    steam_auth_prompt
    console_prompt ()
    {
      steam_auth_prompt p;

      p.request_guard_code =
        [] (const steam_guard_challenge& g) -> asio::awaitable<string>
      {
        string m;

        switch (g.type)
        {
        case steam_guard_type::device_code:
          m = "Enter the Steam Guard code from your Steam Mobile "
              "Authenticator: ";
          break;

        case steam_guard_type::email_code:
          m = g.associated_message.empty ()
                ? string ("Enter the Steam Guard code sent to your email "
                          "address: ")
                : "Enter the Steam Guard code sent to " +
                    g.associated_message + ": ";
          break;

        default:
          m = "Enter your Steam Guard code (" + to_string (g.type) + "): ";
          break;
        }

        co_return read_line (m);
      };

      p.announce_device_confirmation = [] (const steam_guard_challenge& g)
      {
        info ("approve the login in your Steam mobile app{}",
              g.associated_message.empty ()
                ? string ()
                : " (" + g.associated_message + ")");
      };

      p.present_qr_challenge = [] (const string& u)
      {
        info ("scan this code with the Steam mobile app to sign in");
        render_qr (u);
      };

      p.poll_tick = [] ()
      {
        log::trace_l2 (categories::steam (), "waiting for Steam to confirm "
                                             "the login");
      };

      return p;
    }
  }
}
