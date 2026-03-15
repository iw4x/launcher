#include <launcher/http/http-json.hxx>

#include <stdexcept>

namespace launcher
{
  static void
  ensure_success (const http_response& r)
  {
    if (!r.is_success ())
      throw std::runtime_error ("HTTP request failed with status " +
                                std::to_string (r.status_code ()));
  }

  boost::json::value
  parse_json (const http_response& r)
  {
    if (!r.body)
      throw std::runtime_error ("HTTP response has no body to parse as JSON");

    boost::system::error_code ec;
    boost::json::value v (boost::json::parse (*r.body, ec));

    if (ec)
      throw std::runtime_error ("failed to parse JSON response: " +
                                ec.message ());

    return v;
  }

  http_request
  make_json_request (http_method m,
                     const std::string& u,
                     const boost::json::value& j)
  {
    http_request q (m, u);

    q.set_header ("Accept", "application/json");
    q.set_content_type ("application/json");
    q.set_body (boost::json::serialize (j));
    q.normalize ();

    return q;
  }

  json_http_client::
  json_http_client (boost::asio::io_context& c)
    : client_ (c)
  {
  }

  json_http_client::
  json_http_client (boost::asio::io_context& c, const http_client_traits& t)
    : client_ (c, t)
  {
  }

  boost::asio::awaitable<boost::json::value> json_http_client::
  get_json (const std::string& u)
  {
    http_request q (http_method::get, u);
    q.set_header ("Accept", "application/json");
    q.normalize ();

    http_response r (co_await client_.request (q));
    ensure_success (r);

    co_return parse_json (r);
  }

  boost::asio::awaitable<boost::json::value> json_http_client::
  post_json (const std::string& u, const boost::json::value& j)
  {
    http_request q (make_json_request (http_method::post, u, j));
    http_response r (co_await client_.request (q));

    ensure_success (r);
    co_return parse_json (r);
  }

  boost::asio::awaitable<boost::json::value> json_http_client::
  put_json (const std::string& u, const boost::json::value& j)
  {
    http_request q (make_json_request (http_method::put, u, j));
    http_response r (co_await client_.request (q));

    ensure_success (r);
    co_return parse_json (r);
  }

  boost::asio::awaitable<boost::json::value> json_http_client::
  patch_json (const std::string& u, const boost::json::value& j)
  {
    http_request q (make_json_request (http_method::patch, u, j));
    http_response r (co_await client_.request (q));

    ensure_success (r);
    co_return parse_json (r);
  }

  boost::asio::awaitable<boost::json::value> json_http_client::
  delete_json (const std::string& u)
  {
    http_request q (http_method::delete_, u);
    q.set_header ("Accept", "application/json");
    q.normalize ();

    http_response r (co_await client_.request (q));
    ensure_success (r);

    // A DELETE request might return an empty body even on success. Only attempt
    // to parse if there is actually something there.
    //
    if (r.body && !r.body->empty ())
      co_return parse_json (r);

    co_return boost::json::value ();
  }
}
