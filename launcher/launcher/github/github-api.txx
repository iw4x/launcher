#include <stdexcept>

namespace launcher
{
  template <typename T>
  asio::awaitable<typename github_api<T>::response_type> github_api<T>::
  execute (request_type request)
  {
    // Check if we have a cached rate limit and need to wait before proceeding.
    //
    if (last_rate_limit_ && last_rate_limit_->is_exceeded ())
      co_await handle_rate_limit (*last_rate_limit_);

    std::string url (request.url ());
    std::string host (endpoint_type::api_host);
    std::string target (url);

    // Remove https://api.github.com from the beginning.
    //
    const std::string prefix (endpoint_type::api_base);
    if (target.find (prefix) == 0)
      target = target.substr (prefix.length ());

    std::map<std::string, std::string> headers (request.headers);
    add_default_headers (headers);

    if (request.token)
      headers["Authorization"] = "Bearer " + *request.token;

    http::verb verb (http::verb::get);
    switch (request.method)
    {
      case request_type::method_type::get:     verb = http::verb::get;     break;
      case request_type::method_type::post:    verb = http::verb::post;    break;
      case request_type::method_type::put:     verb = http::verb::put;     break;
      case request_type::method_type::patch:   verb = http::verb::patch;   break;
      case request_type::method_type::delete_: verb = http::verb::delete_; break;
    }

    response_type resp (co_await perform_request (host, target, verb, headers, request.body));

    // If the response indicates rate limiting, wait and retry once.
    //
    if (resp.is_rate_limited () && resp.rate_limit && resp.rate_limit->is_exceeded ())
    {
      co_await handle_rate_limit (*resp.rate_limit);
      resp = co_await perform_request (host, target, verb, headers, request.body);
    }

    co_return resp;
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::response_type>
  github_api<T>::
  perform_request (const std::string& host,
                   const std::string& target,
                   http::verb method,
                   const std::map<std::string, std::string>& headers,
                   const std::optional<std::string>& body)
  {
    using tcp = asio::ip::tcp;

    response_type resp;

    try
    {
      tcp::resolver resolver (ioc_);
      auto const results (co_await resolver.async_resolve (
          host, "443", asio::use_awaitable));

      asio::ssl::stream<beast::tcp_stream> stream (ioc_, ssl_ctx_);

      if (!SSL_set_tlsext_host_name (stream.native_handle (), host.c_str ()))
      {
        beast::error_code ec {static_cast<int> (::ERR_get_error ()),
                              asio::error::get_ssl_category ()};
        throw beast::system_error {ec};
      }

      beast::get_lowest_layer (stream).expires_after (std::chrono::seconds (30));
      co_await beast::get_lowest_layer (stream).async_connect (
          results, asio::use_awaitable);

      co_await stream.async_handshake (ssl::stream_base::client, asio::use_awaitable);

      http::request<http::string_body> req {method, target, 11};
      req.set (http::field::host, host);

      for (const auto& [key, value] : headers)
        req.set (key, value);

      if (body)
      {
        req.body () = *body;
        req.prepare_payload ();
      }

      beast::get_lowest_layer (stream).expires_after (std::chrono::seconds (30));
      co_await http::async_write (stream, req, asio::use_awaitable);

      beast::flat_buffer buffer;
      http::response<http::string_body> res;

      co_await http::async_read (stream, buffer, res, asio::use_awaitable);

      resp.status_code = res.result_int ();
      resp.body = res.body ();

      for (const auto& field : res)
        resp.headers[std::string (field.name_string ())] = std::string (field.value ());

      // Extract rate limit information from response headers.
      //
      resp.rate_limit = extract_rate_limit (resp.headers);
      if (resp.rate_limit)
        last_rate_limit_ = *resp.rate_limit;

      if (!resp.success ())
      {
        try
        {
          json::value jv (json::parse (resp.body));
          if (jv.is_object () && jv.as_object ().contains ("message"))
            resp.error_message = json::value_to<std::string> (jv.as_object ().at ("message"));
        }
        catch (...)
        {
          resp.error_message = "HTTP error: " + std::to_string (resp.status_code);
        }
      }

      beast::get_lowest_layer (stream).expires_after (std::chrono::seconds (30));
      beast::error_code ec;
      co_await stream.async_shutdown (asio::redirect_error (asio::use_awaitable, ec));

      // Ignore "short read" errors during shutdown.
      //
      if (ec == asio::error::eof || ec == ssl::error::stream_truncated)
        ec = {};

      if (ec)
        throw beast::system_error {ec};
    }
    catch (const boost::system::system_error& e)
    {
      resp.status_code = 0;
      resp.error_message = std::string (e.what ()) + " [" +
                           e.code ().category ().name () + ":" +
                           std::to_string (e.code ().value ()) + "]";
    }
    catch (const std::exception& e)
    {
      resp.status_code = 0;
      resp.error_message = e.what ();
    }

    co_return resp;
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::repository_type>
  github_api<T>::
  get_repository (const std::string& owner, const std::string& repo)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo (owner, repo));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get repository"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_repository (jv);
  }

  template <typename T>
  asio::awaitable<std::vector<typename github_api<T>::release_type>>
  github_api<T>::
  get_releases (const std::string& owner,
                const std::string& repo,
                std::optional<std::uint32_t> per_page)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_releases (owner, repo));

    if (per_page)
      req.with_per_page (*per_page);

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get releases"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_releases (jv);
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::release_type>
  github_api<T>::
  get_latest_release (const std::string& owner, const std::string& repo)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_release_latest (owner, repo));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get latest release"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_release (jv);
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::release_type>
  github_api<T>::
  get_release_by_tag (const std::string& owner,
                      const std::string& repo,
                      const std::string& tag)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_release_tag (owner, repo, tag));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get release by tag"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_release (jv);
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::release_type>
  github_api<T>::
  get_release_by_id (const std::string& owner,
                     const std::string& repo,
                     std::uint64_t id)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_release_id (owner, repo, id));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get release by ID"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_release (jv);
  }

  template <typename T>
  asio::awaitable<std::vector<typename github_api<T>::commit_type>>
  github_api<T>::
  get_commits (const std::string& owner,
               const std::string& repo,
               std::optional<std::uint32_t> per_page)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_commits (owner, repo));

    if (per_page)
      req.with_per_page (*per_page);

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get commits"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_commits (jv);
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::commit_type>
  github_api<T>::
  get_commit (const std::string& owner,
              const std::string& repo,
              const std::string& sha)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_commit (owner, repo, sha));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get commit"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_commit (jv);
  }

  template <typename T>
  asio::awaitable<std::vector<typename github_api<T>::branch_type>>
  github_api<T>::
  get_branches (const std::string& owner, const std::string& repo)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_branches (owner, repo));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get branches"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_branches (jv);
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::branch_type>
  github_api<T>::
  get_branch (const std::string& owner,
              const std::string& repo,
              const std::string& branch)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_branch (owner, repo, branch));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get branch"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_branch (jv);
  }

  template <typename T>
  asio::awaitable<std::vector<typename github_api<T>::tag_type>>
  github_api<T>::
  get_tags (const std::string& owner, const std::string& repo)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_tags (owner, repo));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get tags"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_tags (jv);
  }

  template <typename T>
  asio::awaitable<std::vector<typename github_api<T>::issue_type>>
  github_api<T>::
  get_issues (const std::string& owner,
              const std::string& repo,
              std::optional<std::string> state)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_issues (owner, repo));

    if (state)
      req.with_state (*state);

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get issues"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_issues (jv);
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::issue_type>
  github_api<T>::
  get_issue (const std::string& owner,
             const std::string& repo,
             std::uint64_t number)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_issue (owner, repo, number));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get issue"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_issue (jv);
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::user_type>
  github_api<T>::
  get_user (const std::string& username)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::user (username));

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get user"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_user (jv);
  }

  template <typename T>
  asio::awaitable<typename github_api<T>::user_type>
  github_api<T>::
  get_authenticated_user ()
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::user_authenticated ());

    response_type resp (co_await execute (std::move (req)));

    if (!resp.success ())
      throw std::runtime_error (resp.error_message.value_or ("Unable to get authenticated user"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_user (jv);
  }

  template <typename T>
  std::optional<github_rate_limit> github_api<T>::
  extract_rate_limit (const std::map<std::string, std::string>& headers)
  {
    // Try to extract the standard GitHub rate limit headers. Note that some
    // responses might not have them (e.g., if we hit some other error or a
    // cached response), but if we see at least one, we'll try to build a valid
    // limit object.
    //
    // https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api
    //
    github_rate_limit l;
    bool found (false);

    auto extract = [&headers] (const char* name, auto& field)
    {
      auto it (headers.find (name));
      if (it != headers.end ())
      {
        try
        {
          // stoul/stoull are usually fine here since the headers are
          // numeric. If they are garbage, we just ignore them.
          //
          if constexpr (std::is_same_v<decltype (field), std::uint64_t>)
            field = std::stoull (it->second);
          else
            field = std::stoul (it->second);

          return true;
        }
        catch (...) {}
      }
      return false;
    };

    if (extract ("x-ratelimit-limit", l.limit))         found = true;
    if (extract ("x-ratelimit-remaining", l.remaining)) found = true;
    if (extract ("x-ratelimit-reset", l.reset))         found = true;
    if (extract ("x-ratelimit-used", l.used))           found = true;

    return found ? std::optional<github_rate_limit> (l) : std::nullopt;
  }

  template <typename T>
  asio::awaitable<void> github_api<T>::
  handle_rate_limit (const github_rate_limit& limit)
  {
    if (limit.is_exceeded ())
    {
      // We have hit the ceiling. Calculate how long we need to wait and add
      // a small buffer (1 second) just to be on the safe side and ensure
      // the server has definitely rolled over.
      //
      auto wait (limit.seconds_until_reset () + 1);

      asio::steady_timer timer (ioc_);

      // If the caller provided a progress callback, we can be nice and show a
      // countdown so the user knows we haven't hung.
      //
      if (progress_callback_)
      {
        progress_callback_ (
          "GitHub API rate limit exceeded. Waiting for reset...",
          wait);

        // Spin the loop every second to update the status.
        //
        for (auto rem (wait); rem > 0; --rem)
        {
          timer.expires_after (std::chrono::seconds (1));
          co_await timer.async_wait (asio::use_awaitable);

          progress_callback_ (
            "GitHub API rate limit exceeded. Waiting for reset...",
            rem - 1);
        }
      }
      // Otherwise just sleep efficiently for the whole duration.
      //
      else
      {
        timer.expires_after (std::chrono::seconds (wait));
        co_await timer.async_wait (asio::use_awaitable);
      }
    }
  }
}
