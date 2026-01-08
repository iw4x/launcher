#include <hello/hello-http.hxx>

#include <boost/json.hpp>

#include <sstream>
#include <stdexcept>

#include <hello/http/http-client.hxx>
#include <hello/http/http-json.hxx>

using namespace std;

namespace hello
{
  namespace json = boost::json;

  http_coordinator::
  http_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      client_ (make_unique<client_type> (ioc))
  {
  }

  http_coordinator::
  http_coordinator (asio::io_context& ioc,
                    const http_client_traits<>& traits)
    : ioc_ (ioc),
      client_ (make_unique<client_type> (ioc, traits))
  {
  }

  asio::awaitable<string> http_coordinator::
  get (const string& url)
  {
    response_type r (co_await client_->get (url));

    if (r.is_error ())
      throw runtime_error (format_http_error (r));

    // Handle empty bodies gracefully.
    //
    if (!r.body)
      co_return string ();

    co_return *r.body;
  }

  asio::awaitable<http_coordinator::response_type> http_coordinator::
  get_response (const string& url)
  {
    // Pass the raw response back to the caller; sometimes they need headers
    // or status codes specifically.
    //
    response_type r (co_await client_->get (url));

    if (r.is_error ())
      throw runtime_error (format_http_error (r));

    co_return r;
  }

  asio::awaitable<string> http_coordinator::
  post_json (const string& url, const string& json)
  {
    response_type r (co_await client_->post (url,
                                             json,
                                             "application/json"));

    if (r.is_error ())
      throw runtime_error (format_http_error (r));

    if (!r.body)
      co_return string ();

    co_return *r.body;
  }

  asio::awaitable<uint64_t> http_coordinator::
  download_file (const string& url,
                 const fs::path& target,
                 progress_callback cb,
                 optional<uint64_t> resume)
  {
    // Note that the destination directory *must* exists. That is, the
    // lower-level client might fail or (worse) do nothing if the parent dir is
    // missing.
    //
    if (target.has_parent_path ())
    {
      error_code ec;
      fs::create_directories (target.parent_path (), ec);

      if (ec)
        throw runtime_error ("failed to create target directory: " +
                             ec.message ());
    }

    // Adapt the coordinator callback to the client callback if necessary.
    //
    http_client::progress_callback adapted;

    if (cb)
    {
      adapted = [cb] (uint64_t d, uint64_t t)
      {
        cb (d, t);
      };
    }

    uint64_t n (co_await client_->download (url,
                                            target.string (),
                                            adapted,
                                            resume));
    co_return n;
  }

  asio::awaitable<optional<uint64_t>> http_coordinator::
  get_content_length (const string& url)
  {
    // Just a HEAD request. We don't want the body.
    //
    response_type r (co_await client_->head (url));

    if (r.is_error ())
      throw runtime_error (format_http_error (r));

    co_return r.content_length ();
  }

  asio::awaitable<bool> http_coordinator::
  check_url (const string& url)
  {
    // A quick reachability test. We swallow exceptions here because a failure
    // just means "not available" in this context.
    //
    try
    {
      response_type r (co_await client_->head (url));
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
    return *client_;
  }

  const http_coordinator::client_type& http_coordinator::
  client () const noexcept
  {
    return *client_;
  }

  // Helpers
  //

  json::value
  parse_json (const string& b)
  {
    error_code ec;
    json::value jv (json::parse (b, ec));

    if (ec)
      throw runtime_error ("failed to parse JSON: " + ec.message ());

    return jv;
  }

  string
  format_http_error (const http_response& r)
  {
    ostringstream oss;
    oss << "HTTP " << r.status_code () << " " << r.reason;

    // If the server sent a body (e.g., a detailed error message), include it,
    // but don't spam the logs if it sent back an entire HTML 404 page.
    //
    if (r.body && !r.body->empty ())
    {
      const string& b (*r.body);
      const size_t max (200);

      if (b.size () <= max)
        oss << ": " << b;
      else
        oss << ": " << b.substr (0, max) << "...";
    }

    return oss.str ();
  }
}
