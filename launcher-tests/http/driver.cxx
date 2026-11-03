// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <map>
#include <print>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>      // exit()
#include <fstream>
#include <utility>      // move()
#include <iostream>
#include <iterator>
#include <optional>
#include <exception>
#include <filesystem>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <launcher/http/http-client.hxx>

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

  // Canned response for a target.
  //
  struct route
  {
    unsigned status;
    string text; // Location for a redirect, body for any other status.
  };

  // Loopback HTTP server that answers every request with its route's
  // response (404 for an unknown target) and then closes the connection.
  //
  // Note that a route with the 200 status honors the open-ended range that
  // download resumption requests (bytes=<offset>-) with the 206 response.
  // Note also that we make a redirect location that starts with '/'
  // absolute.
  //
  class server
  {
  public:
    server (asio::io_context& ioc, map<string, route> rs)
      : acceptor_ (ioc, tcp::endpoint (asio::ip::address_v4::loopback (), 0)),
        routes_ (move (rs))
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

    // Return the requests received so far in the <method> <target> form,
    // followed by range=<value> if the request has the Range header.
    //
    const vector<string>&
    log () const noexcept {return log_;}

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

      string t (rq.target ());
      string range (rq[http::field::range]);

      log_.push_back (string (rq.method_string ()) + ' ' + t +
                      (range.empty () ? "" : " range=" + range));

      auto i (routes_.find (t));
      route r (i != routes_.end () ? i->second : route {404, "not found"});

      http::response<http::string_body> rs;
      rs.version (11);
      rs.keep_alive (false);
      rs.result (r.status);

      if (r.status >= 300 && r.status < 400)
      {
        rs.set (http::field::location,
                r.text.starts_with ('/') ? url (r.text) : r.text);
      }
      else if (r.status == 200 && range.starts_with ("bytes="))
      {
        size_t o (stoul (range.substr (6)));

        rs.result (http::status::partial_content);
        rs.set (http::field::content_range,
                "bytes " + to_string (o) + '-' +
                to_string (r.text.size () - 1) + '/' +
                to_string (r.text.size ()));
        rs.body () = r.text.substr (o);
      }
      else
        rs.body () = r.text;

      rs.prepare_payload ();

      co_await http::async_write (s, rs, asio::use_awaitable);

      boost::system::error_code ec;
      s.shutdown (tcp::socket::shutdown_both, ec);
    }

    tcp::acceptor acceptor_;
    map<string, route> routes_;
    vector<string> log_;
  };

  // Read the routes from stdin (see main() for the form).
  //
  map<string, route>
  read_routes ()
  {
    map<string, route> r;

    for (string l; getline (cin, l); )
    {
      size_t p (l.find (' '));
      size_t q (p != string::npos ? l.find (' ', p + 1) : string::npos);

      if (p == string::npos)
        fail ("invalid route '" + l + "'");

      unsigned s (stoul (l.substr (p + 1, q - p - 1)));

      r[l.substr (0, p)] = route {
        s, q != string::npos ? l.substr (q + 1) : string ()};
    }

    return r;
  }
}

// Usage: argv[0] <command> <target> [<argument>]
//
// Start a loopback HTTP server with the routes read from stdin, one per
// line, in the following form, where the text is the location for a
// redirect status and the body for any other:
//
//    <target> <status> [<text>]
//
// Then run the command against the target on that server with the
// launcher's HTTP client and print its result to stdout, followed by the
// requests that the server received, each prefixed with '> '.
//
// get <target>
//    Print the response status and its body, if any.
//
// post <target> <body>
//    As get but send a POST request with the body.
//
// download <target> [<offset>]
//    Download the target into the download file in the current working
//    directory, resuming at the offset if specified. Print the number of
//    bytes downloaded and then the file contents.
//
int
main (int argc, char* argv[])
{
  vector<string> as (argv + 1, argv + argc);

  if (as.size () < 2 || as.size () > 3)
    fail ("invalid arguments\n"
          "  info: usage: driver <command> <target> [<argument>]");

  const string& c (as[0]);

  asio::io_context ioc;
  server sv (ioc, read_routes ());
  http_client hc (ioc);

  const string u (sv.url (as[1]));
  optional<string> r;
  exception_ptr e;

  auto perform = [&c, &as, &u, &hc] () -> asio::awaitable<string>
  {
    if (c == "get" || c == "post")
    {
      http_response rs (c == "get"
                        ? co_await hc.get (u)
                        : co_await hc.post (u, as.size () == 3 ? as[2] : ""));

      co_return "status " + to_string (rs.status_code ()) +
                (rs.body ? "\nbody " + *rs.body : "");
    }

    if (c == "download")
    {
      const filesystem::path f ("download");

      optional<uint64_t> o;
      if (as.size () == 3)
        o = stoull (as[2]);

      uint64_t n (co_await hc.download (u, f, nullptr, o));

      ifstream is (f, ios::binary);
      string s ((istreambuf_iterator<char> (is)), istreambuf_iterator<char> ());

      co_return "bytes " + to_string (n) + "\nfile " + s;
    }

    throw invalid_argument ("invalid command '" + c + "'");
  };

  asio::co_spawn (ioc,
                  perform (),
                  [&ioc, &r, &e] (exception_ptr x, string s)
  {
    e = move (x);
    r = move (s);
    ioc.stop ();
  });

  ioc.run ();

  if (r && e == nullptr)
    println ("{}", *r);

  for (const string& l: sv.log ())
    println ("> {}", l);

  if (e != nullptr)
  {
    try
    {
      rethrow_exception (e);
    }
    catch (const exception& x)
    {
      fail (x.what ());
    }
  }
}
