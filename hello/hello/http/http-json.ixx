#include <stdexcept>

namespace hello
{
  // parse_json
  //
  template <typename S>
  inline boost::json::value
  parse_json (const basic_http_response<S>& response)
  {
    if (!response.body)
      throw std::runtime_error ("HTTP response has no body to parse as JSON");

    boost::system::error_code ec;
    boost::json::value result (boost::json::parse (*response.body, ec));

    if (ec)
      throw std::runtime_error (
        "failed to parse JSON response: " + ec.message ());

    return result;
  }

  // make_json_request
  //
  template <typename S>
  inline basic_http_request<S>
  make_json_request (http_method method,
                     const S& url,
                     const boost::json::value& json)
  {
    basic_http_request<S> req (method, url);
    req.set_content_type (S ("application/json"));
    req.set_body (boost::json::serialize (json));
    req.normalize ();
    return req;
  }

  // basic_json_http_client
  //
  template <typename T>
  inline boost::asio::awaitable<boost::json::value>
  basic_json_http_client<T>::
  get_json (const string_type& url)
  {
    request_type req (http_method::get, url);
    req.set_header (string_type ("Accept"), string_type ("application/json"));
    req.normalize ();

    response_type res (co_await client_.request (req));

    if (!res.is_success ())
    {
      throw std::runtime_error (
        "HTTP request failed with status " +
        std::to_string (res.status_code ()));
    }

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
    {
      throw std::runtime_error (
        "HTTP request failed with status " +
        std::to_string (res.status_code ()));
    }

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
    {
      throw std::runtime_error (
        "HTTP request failed with status " +
        std::to_string (res.status_code ()));
    }

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
    {
      throw std::runtime_error (
        "HTTP request failed with status " +
        std::to_string (res.status_code ()));
    }

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
    {
      throw std::runtime_error (
        "HTTP request failed with status " +
        std::to_string (res.status_code ()));
    }

    // DELETE may not return a body.
    //
    if (res.body && !res.body->empty ())
      co_return parse_json (res);

    co_return boost::json::value ();
  }
}
