// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <print>
#include <string>
#include <vector>
#include <cstdlib>      // exit()
#include <utility>      // move()
#include <iostream>
#include <algorithm>    // ranges::sort()
#include <exception>
#include <filesystem>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <launcher/launcher-cache.hxx>
#include <launcher/launcher-download.hxx>
#include <launcher/launcher-progress.hxx>
#include <launcher/manifest/manifest.hxx>
#include <launcher/progress/progress-manager.hxx>

using namespace std;
using namespace launcher;

namespace
{
  namespace http = boost::beast::http;

  using tcp = asio::ip::tcp;

  [[noreturn]] void
  fail (const string& m)
  {
    println (cerr, "error: {}", m);
    exit (1);
  }

  // Loopback HTTP server that answers every request with the target, without
  // the leading '/', as the body and then closes the connection.
  //
  class server
  {
  public:
    explicit
    server (asio::io_context& ioc)
      : acceptor_ (ioc, tcp::endpoint (asio::ip::address_v4::loopback (), 0))
    {
      asio::co_spawn (ioc, accept (), asio::detached);
    }

    // Return the URL of the target on this server.
    //
    string
    url (const string& t) const
    {
      return "http://127.0.0.1:" +
             to_string (acceptor_.local_endpoint ().port ()) + t;
    }

  private:
    asio::awaitable<void>
    accept ()
    {
      for (;;)
      {
        tcp::socket s (co_await acceptor_.async_accept (asio::use_awaitable));
        asio::co_spawn (acceptor_.get_executor (),
                        serve (move (s)),
                        asio::detached);
      }
    }

    asio::awaitable<void>
    serve (tcp::socket s)
    {
      boost::beast::flat_buffer b;
      http::request<http::string_body> rq;

      co_await http::async_read (s, b, rq, asio::use_awaitable);

      http::response<http::string_body> rs;
      rs.version (11);
      rs.keep_alive (false);
      rs.result (http::status::ok);
      rs.body () = string (rq.target ()).substr (1);
      rs.prepare_payload ();

      co_await http::async_write (s, rs, asio::use_awaitable);

      boost::system::error_code ec;
      s.shutdown (tcp::socket::shutdown_both, ec);
    }

    tcp::acceptor acceptor_;
  };
}

// Usage: argv[0] <command> [<name>...]
//
// Run the command on the rawfiles component of the installation in the
// current directory.
//
// sync <name>...
//    Bring the component up to date with a manifest of the named files,
//    which a loopback HTTP server serves with the name as the content. Then
//    print the outcome and the label of each progress entry, sorted, in the
//    following form:
//
//    status <status>
//    entry <label>
//
// installed
//    Print whether the component is installed.
//
int
main (int argc, char* argv[])
try
{
  if (argc < 2)
    fail ("command expected");

  const string c (argv[1]);

  asio::io_context ioc;
  filesystem::path d (filesystem::current_path ());

  if (c == "installed")
  {
    if (argc != 2)
      fail ("unexpected argument");

    cache_coordinator cc (d);
    cout << (cc.installed (component_type::rawfiles) ? "true" : "false")
         << '\n';
    return 0;
  }

  if (c != "sync")
    fail ("unknown command '" + c + "'");

  if (argc < 3)
    fail ("file name expected");

  server srv (ioc);

  manifest m;

  for (int i (2); i != argc; ++i)
  {
    string n (argv[i]);

    manifest_archive a;
    a.name = n;
    a.url = srv.url ('/' + n);
    a.size = n.size ();

    m.archives.push_back (move (a));
  }

  download_coordinator dc (ioc);
  progress_coordinator pc (ioc);
  cache_coordinator cc (d);

  cc.set_download_coordinator (&dc);
  cc.set_progress_coordinator (&pc);

  cache_result r;
  exception_ptr e;

  asio::co_spawn (ioc,
                  cc.sync (m, component_type::rawfiles, "1.0.0"),
                  [&ioc, &r, &e] (exception_ptr x, cache_result v)
  {
    e = x;
    r = move (v);
    ioc.stop ();
  });

  ioc.run ();

  if (e)
    rethrow_exception (e);

  // The entries change once their work runs, so run what is still pending.
  //
  ioc.restart ();
  ioc.poll ();

  vector<string> ls;

  for (const auto& p : pc.manager ().entries ())
    ls.push_back (p->label ());

  ranges::sort (ls);

  cout << "status " << r.status << '\n';

  for (const string& l : ls)
    cout << "entry " << l << '\n';
}
catch (const exception& e)
{
  fail (e.what ());
}
