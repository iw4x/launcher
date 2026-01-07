#include <hello/hello-http.hxx>

#include <hello/http/http-client.hxx>
#include <hello/http/http-json.hxx>

#include <boost/json.hpp>

#include <stdexcept>
#include <sstream>

namespace hello
{
  namespace json = boost::json;

  http_coordinator::
  http_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      client_ (std::make_unique<client_type> (ioc))
  {
  }

  http_coordinator::
  http_coordinator (asio::io_context& ioc,
                    const http_client_traits<>& traits)
      : ioc_ (ioc),
        client_ (std::make_unique<client_type> (ioc, traits))
  {
  }

  asio::awaitable<std::string> http_coordinator::
  get (const std::string& url)
  {
    // Perform GET request and return body.
    //
    response_type response (co_await client_->get (url));

    // Check for HTTP errors.
    //
    if (response.is_error ())
      throw std::runtime_error (format_http_error (response));

    // Extract body.
    //
    if (!response.body)
      co_return std::string ();

    co_return *response.body;
  }

  asio::awaitable<http_coordinator::response_type> http_coordinator::
  get_response (const std::string& url)
  {
    response_type response (co_await client_->get (url));

    if (response.is_error ())
      throw std::runtime_error (format_http_error (response));

    co_return response;
  }

  asio::awaitable<std::string> http_coordinator::
  post_json (const std::string& url, const std::string& json_body)
  {
    response_type response (
      co_await client_->post (url, json_body, "application/json"));

    if (response.is_error ())
      throw std::runtime_error (format_http_error (response));

    if (!response.body)
      co_return std::string ();

    co_return *response.body;
  }

  asio::awaitable<std::uint64_t> http_coordinator::
  download_file (const std::string& url,
                 const fs::path& target,
                 progress_callback progress,
                 std::optional<std::uint64_t> resume_from)
  {
    if (target.has_parent_path ())
    {
      std::error_code ec;
      fs::create_directories (target.parent_path (), ec);

      if (ec)
        throw std::runtime_error ("failed to create target directory: " +
                                  ec.message ());
    }

    http_client::progress_callback adapted_progress;

    if (progress)
    {
      adapted_progress =
        [progress] (std::uint64_t transferred, std::uint64_t total)
      {
        progress (transferred, total);
      };
    }

    std::uint64_t bytes_downloaded (
      co_await client_->download (url,
                                  target.string (),
                                  adapted_progress,
                                  resume_from));

    co_return bytes_downloaded;
  }

  asio::awaitable<std::optional<std::uint64_t>> http_coordinator::
  get_content_length (const std::string& url)
  {
    response_type response (co_await client_->head (url));

    if (response.is_error ())
      throw std::runtime_error (format_http_error (response));

    co_return response.content_length ();
  }

  asio::awaitable<bool> http_coordinator::
  check_url (const std::string& url)
  {
    try
    {
      response_type response (co_await client_->head (url));
      co_return response.is_success ();
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

  json::value
  parse_json (const std::string& body)
  {
    // Parse JSON from string.
    //
    std::error_code ec;
    json::value jv (json::parse (body, ec));

    if (ec)
      throw std::runtime_error ("failed to parse JSON: " + ec.message ());

    return jv;
  }

  // Error formatting.
  //

  std::string
  format_http_error (const http_response& response)
  {
    // Build a descriptive error message from the HTTP response.
    //
    std::ostringstream oss;
    oss << "HTTP " << response.status_code () << " " << response.reason;

    if (response.body && !response.body->empty ())
    {
      const std::string& body (*response.body);
      const std::size_t max_body_len (200);

      if (body.size () <= max_body_len)
        oss << ": " << body;
      else
        oss << ": " << body.substr (0, max_body_len) << "...";
    }

    return oss.str ();
  }
}
