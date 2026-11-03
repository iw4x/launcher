// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cerrno>
#include <chrono>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include <boost/asio/post.hpp>
#include <boost/asio/config.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/steady_timer.hpp>

#undef NDEBUG
#include <cassert>

using namespace std;

namespace asio = boost::asio;

namespace
{
  // Look up the key in the test section, starting with errno set to ERANGE
  // (see main()), and return its value as text. Return the default if there
  // is no such key.
  //
  template <typename T>
  string
  get (const asio::config& c, const string& k, T d)
  {
    errno = ERANGE;
    T v (c.get ("test", k.c_str (), d));

    if constexpr (is_same_v<T, bool>)
      return v ? "true" : "false";
    else
      return to_string (+v);
  }
}

// Usage: argv[0] <command> [<argument>...]
//
// Exercise the Asio configuration parser, which before Boost 1.88 rejected
// valid values if errno was already set and truncated concurrency hints.
// Each step starts with errno set to ERANGE since a successful library call
// may leave it that way and the parser should not take it for a failure of
// its own conversion.
//
// context
//    Create an I/O context with its services and run a timer on it.
//
// pool <threads>
//    Create a thread pool, print its concurrency hint, and run a function
//    on it.
//
// hint <concurrency-hint>
//    Create an execution context with the hint and print the hint that its
//    configuration reports.
//
// get (<type> <key>)...
//    Read a configuration in the <section>.<key>=<value> form from stdin
//    and print the value of each key in the test section converted to the
//    type, one per line, with 'out of range' standing for a value that
//    doesn't fit. The type is one of int, uint, bool, llong, ullong, and
//    uchar. A missing key yields the default, which is 123 (true for bool).
//
int
main (int argc, char* argv[])
{
  string_view c (argc >= 2 ? argv[1] : "");

  if (c == "context" && argc == 2)
  {
    errno = ERANGE;
    asio::io_context io;

    errno = ERANGE;
    asio::steady_timer t (io, chrono::milliseconds (0));

    errno = ERANGE;
    asio::ip::tcp::resolver r (io);

    errno = ERANGE;
    asio::ip::tcp::socket s (io);

    bool done (false);
    t.async_wait ([&done] (const boost::system::error_code& ec)
    {
      assert (!ec);
      done = true;
    });

    io.run ();
    assert (done);
  }
  else if (c == "pool" && argc == 3)
  {
    errno = ERANGE;
    asio::thread_pool p (stoul (argv[2]));

    errno = ERANGE;
    cout << asio::config (p).get ("scheduler", "concurrency_hint", -1) << endl;

    bool done (false);
    asio::post (p, [&done] () {done = true;});
    p.join ();
    assert (done);
  }
  else if (c == "hint" && argc == 3)
  {
    errno = ERANGE;
    asio::execution_context x (asio::config_from_concurrency_hint {
                                 stoi (argv[2])});

    errno = ERANGE;
    cout << asio::config (x).get ("scheduler", "concurrency_hint", 0) << endl;
  }
  else if (c == "get" && argc >= 4 && argc % 2 == 0)
  {
    ostringstream is;
    is << cin.rdbuf ();

    asio::execution_context x (asio::config_from_string {is.str ()});
    asio::config cf (x);

    for (int i (2); i != argc; i += 2)
    {
      const string t (argv[i]);
      const string k (argv[i + 1]);

      string v;

      try
      {
        if      (t == "int")    v = get (cf, k, 123);
        else if (t == "uint")   v = get (cf, k, 123U);
        else if (t == "bool")   v = get (cf, k, true);
        else if (t == "llong")  v = get (cf, k, 123LL);
        else if (t == "ullong") v = get (cf, k, 123ULL);
        else if (t == "uchar")  v = get<unsigned char> (cf, k, 123);
        else
        {
          cerr << "error: invalid type '" << t << "'" << endl;
          return 1;
        }
      }
      catch (const out_of_range&)
      {
        v = "out of range";
      }

      cout << v << endl;
    }
  }
  else
  {
    cerr << "error: invalid arguments\n"
         << "  info: usage: driver <command> [<argument>...]" << endl;
    return 1;
  }
}
