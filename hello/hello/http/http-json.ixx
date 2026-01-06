#include <stdexcept>

namespace hello
{
  // Parse the HTTP response body as JSON.
  //
  // We expect the response to actually contain data. If the body is missing
  // (e.g., a HEAD response) or empty, we treat that as a usage error.
  //
  template <typename S>
  inline boost::json::value
  parse_json (const basic_http_response<S>& r)
  {
    if (!r.body)
      throw std::runtime_error ("HTTP response has no body to parse as JSON");

    // We use the error_code overload here to avoid exceptions from the
    // parsing library itself, preferring to wrap the error in our own
    // exception for consistency with the rest of the client API.
    //
    boost::system::error_code ec;
    boost::json::value v (boost::json::parse (*r.body, ec));

    if (ec)
      throw std::runtime_error ("failed to parse JSON response: " +
                                ec.message ());

    return v;
  }

  // Helper to construct a JSON request.
  //
  template <typename S>
  inline basic_http_request<S>
  make_json_request (http_method m,
                     const S& url,
                     const boost::json::value& j)
  {
    basic_http_request<S> r (m, url);
    r.set_content_type (S ("application/json"));
    r.set_body (boost::json::serialize (j));
    r.normalize ();
    return r;
  }

  template <typename T>
  inline boost::asio::awaitable<boost::json::value>
  basic_json_http_client<T>::
  get_json (const string_type& url)
  {
    // Explicitly ask for JSON. Most APIs require this to avoid returning XML
    // or HTML by default.
    //
    request_type req (http_method::get, url);
    req.set_header (string_type ("Accept"), string_type ("application/json"));
    req.normalize ();

    response_type res (co_await client_.request (req));

    // Unlike the raw client which might be used to inspect 404s, here we treats
    // any non-success status as a failure to retrieve the data.
    //
    if (!res.is_success ())
      throw std::runtime_error ("HTTP request failed with status " +
                                std::to_string (res.status_code ()));

    co_return parse_json (res);
  }

  template <typename T>
  inline boost::asio::awaitable<boost::json::value>
  basic_json_http_client<T>::
  post_json (const string_type& url,
             const boost::json::value& json)
  {
    request_type req (make_json_request (http_method::post, url, json));
    response_type res (co_await client_.request (req));

    if (!res.is_success ())
      throw std::runtime_error ("HTTP request failed with status " +
                                std::to_string (res.status_code ()));

    co_return parse_json (res);
  }

  template <typename T>
  inline boost::asio::awaitable<boost::json::value>
  basic_json_http_client<T>::
  put_json (const string_type& url,
            const boost::json::value& json)
  {
    request_type req (make_json_request (http_method::put, url, json));
    response_type res (co_await client_.request (req));

    if (!res.is_success ())
      throw std::runtime_error ("HTTP request failed with status " +
                                std::to_string (res.status_code ()));

    co_return parse_json (res);
  }

  template <typename T>
  inline boost::asio::awaitable<boost::json::value>
  basic_json_http_client<T>::
  patch_json (const string_type& url,
              const boost::json::value& json)
  {
    request_type req (make_json_request (http_method::patch, url, json));
    response_type res (co_await client_.request (req));

    if (!res.is_success ())
      throw std::runtime_error ("HTTP request failed with status " +
                                std::to_string (res.status_code ()));

    co_return parse_json (res);
  }

  template <typename T>
  inline boost::asio::awaitable<boost::json::value>
  basic_json_http_client<T>::
  delete_json (const string_type& url)
  {
    request_type req (http_method::delete_, url);
    req.set_header (string_type ("Accept"), string_type ("application/json"));
    req.normalize ();

    response_type res (co_await client_.request (req));

    if (!res.is_success ())
      throw std::runtime_error ("HTTP request failed with status " +
                                std::to_string (res.status_code ()));

    // Special case for DELETE: The server might return 204 No Content (empty
    // body) or a 200 OK with a JSON payload. We must check before trying to
    // parse.
    //
    if (res.body && !res.body->empty ())
      co_return parse_json (res);

    co_return boost::json::value ();
  }
}
