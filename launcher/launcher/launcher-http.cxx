// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/launcher-http.hxx>

#include <sstream>
#include <stdexcept>

#include <boost/json.hpp>

#include <launcher/http/http-client.hxx>
#include <launcher/http/http-json.hxx>

using namespace std;

namespace launcher
{
  namespace json = boost::json;

  static json::value
  parse_json (const string& s)
  {
    boost::system::error_code e;
    json::value v (json::parse (s, e));

    if (e)
      throw runtime_error ("failed to parse JSON: " + e.message ());

    return v;
  }

  // Describe the error response for an exception message.
  //
  static string
  fmt_err (const http_response& r)
  {
    ostringstream o;
    o << "HTTP " << r.status_code () << " " << r.reason;

    // Include the start of the body, which explains the error, and leave
    // out the rest, which can be a whole HTML page.
    //
    if (r.body && !r.body->empty ())
    {
      const string& b (*r.body);
      const size_t m (200);

      if (b.size () <= m)
        o << ": " << b;
      else
        o << ": " << b.substr (0, m) << "...";
    }

    return o.str ();
  }

  http_coordinator::http_coordinator (asio::io_context& i)
    : ioc_ (i),
      client_ (i) {}

  http_coordinator::http_coordinator (asio::io_context& i,
                                      const http_client_traits& t)
    : ioc_ (i),
      client_ (i, t) {}

  asio::awaitable<string> http_coordinator::
  get (const string& u)
  {
    response_type r (co_await client_.get (u));

    if (r.is_error ())
      throw runtime_error (fmt_err (r));

    if (!r.body)
      co_return string ();

    co_return *r.body;
  }

  asio::awaitable<http_coordinator::response_type> http_coordinator::
  get_response (const string& u)
  {
    response_type r (co_await client_.get (u));

    if (r.is_error ())
      throw runtime_error (fmt_err (r));

    co_return r;
  }

  asio::awaitable<string> http_coordinator::
  post_json (const string& u, const string& j)
  {
    response_type r (co_await client_.post (u, j, "application/json"));

    if (r.is_error ())
      throw runtime_error (fmt_err (r));

    if (!r.body)
      co_return string ();

    co_return *r.body;
  }

  asio::awaitable<uint64_t> http_coordinator::
  download_file (const string& u,
                 const fs::path& t,
                 progress_callback cb,
                 optional<uint64_t> rs)
  {
    if (t.has_parent_path ())
    {
      std::error_code e;
      fs::create_directories (t.parent_path (), e);

      if (e)
        throw runtime_error ("failed to create target directory: " +
                             e.message ());
    }

    http_client::progress_callback acb;

    if (cb)
    {
      acb = [cb] (uint64_t d, uint64_t tot)
      {
        cb (d, tot);
      };
    }

    uint64_t n (co_await client_.download (u, t, acb, rs));
    co_return n;
  }

  asio::awaitable<optional<uint64_t>> http_coordinator::
  get_content_length (const string& u)
  {
    response_type r (co_await client_.head (u));

    if (r.is_error ())
      throw runtime_error (fmt_err (r));

    co_return r.content_length ();
  }

  asio::awaitable<bool> http_coordinator::
  check_url (const string& u)
  {
    try
    {
      response_type r (co_await client_.head (u));
      co_return r.is_success ();
    }
    catch (...)
    {
      co_return false;
    }
  }

  http_coordinator::client_type& http_coordinator::
  client () noexcept
  {
    return client_;
  }

  const http_coordinator::client_type& http_coordinator::
  client () const noexcept
  {
    return client_;
  }
}
