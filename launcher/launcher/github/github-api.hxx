// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <map>
#include <optional>
#include <vector>
#include <functional>

#include <boost/asio.hpp>
#include <boost/json.hpp>

#include <launcher/http/http-client.hxx>

#include <launcher/github/github-types.hxx>
#include <launcher/github/github-endpoint.hxx>
#include <launcher/github/github-request.hxx>

namespace launcher
{
  namespace asio = boost::asio;
  namespace json = boost::json;

  // Rate limit as reported in the x-ratelimit-* response headers.
  //
  struct github_rate_limit
  {
    std::uint32_t limit;       // Requests per window.
    std::uint32_t remaining;   // Requests left in the window.
    std::uint64_t reset;       // End of the window as a UNIX time.
    std::uint32_t used;        // Requests made in the window.

    github_rate_limit ();

    // Return true if there are no requests left.
    //
    bool
    is_exceeded () const;

    // Return the number of seconds until the end of the window or 0 if it
    // has already ended.
    //
    std::uint64_t
    seconds_until_reset () const;
  };

  // GitHub API response.
  //
  struct github_response
  {
    unsigned status_code; // 0 if the request itself failed.
    std::string body;
    std::map<std::string, std::string> headers; // Names in lower case.
    std::optional<std::string> error_message;
    std::optional<github_rate_limit> rate_limit;

    // Return true if the status is 2xx.
    //
    bool
    success () const;

    bool
    empty () const;

    // Return true if the status is 403 or 429.
    //
    // Note that GitHub responds with 403 to exceeding the primary rate limit
    // and with 403 or 429 to exceeding a secondary one, but 403 can also
    // mean that the request is forbidden for other reasons.
    //
    bool
    is_rate_limited () const;
  };

  // Entity types and their JSON conversion.
  //
  // Note that the parse functions return a default entity if the value is
  // not an object (or an empty vector if not an array) and throw
  // boost::system::system_error if a field has a wrong type.
  //
  struct github_api_traits
  {
    using request_type = github_request;
    using response_type = github_response;
    using endpoint_type = github_endpoint;

    using user_type = github_user;
    using repository_type = github_repository;
    using release_type = github_release;
    using asset_type = github_asset;
    using commit_type = github_commit;
    using issue_type = github_issue;
    using branch_type = github_branch;
    using tag_type = github_tag;

    static user_type
    parse_user (const json::value& jv);

    static repository_type
    parse_repository (const json::value& jv);

    static release_type
    parse_release (const json::value& jv);

    static asset_type
    parse_asset (const json::value& jv);

    static commit_type
    parse_commit (const json::value& jv);

    static issue_type
    parse_issue (const json::value& jv);

    static branch_type
    parse_branch (const json::value& jv);

    static tag_type
    parse_tag (const json::value& jv);

    static std::vector<release_type>
    parse_releases (const json::value& jv);

    static std::vector<commit_type>
    parse_commits (const json::value& jv);

    static std::vector<branch_type>
    parse_branches (const json::value& jv);

    static std::vector<tag_type>
    parse_tags (const json::value& jv);

    static std::vector<issue_type>
    parse_issues (const json::value& jv);

    static json::value
    to_json (const user_type& u);

    static json::value
    to_json (const repository_type& r);

    static json::value
    to_json (const release_type& r);

    // Return the User-Agent header value, which GitHub requires.
    //
    static std::string
    user_agent ();

    // Return the X-GitHub-Api-Version header value.
    //
    static std::string
    api_version ();
  };

  // GitHub REST API client.
  //
  // The client waits out the rate limit: before a request if the previous
  // response exhausted it, and after a rate limited response, in which case
  // it retries the request once.
  //
  // The get_*() functions throw std::runtime_error with the GitHub error
  // message if the request fails.
  //
  class github_api
  {
  public:
    using traits_type = github_api_traits;
    using request_type = traits_type::request_type;
    using response_type = traits_type::response_type;
    using endpoint_type = traits_type::endpoint_type;

    using user_type = traits_type::user_type;
    using repository_type = traits_type::repository_type;
    using release_type = traits_type::release_type;
    using asset_type = traits_type::asset_type;
    using commit_type = traits_type::commit_type;
    using issue_type = traits_type::issue_type;
    using branch_type = traits_type::branch_type;
    using tag_type = traits_type::tag_type;

  public:
    explicit
    github_api (asio::io_context& ioc);

    github_api (asio::io_context& ioc, std::string token);

    github_api (const github_api&) = delete;
    github_api& operator= (const github_api&) = delete;

    // Set the token that we send with every request as a bearer token.
    //
    void
    set_token (std::string token);

    void
    set_proxy (std::string proxy_url);

    // Called every second while we wait out the rate limit.
    //
    using progress_callback_type =
      std::function<void (const std::string& message, std::uint64_t seconds_remaining)>;

    void
    set_progress_callback (progress_callback_type callback);

    // Perform the request and return the response. Note that a failed
    // request is reported in the response (see github_response for details).
    //
    asio::awaitable<response_type>
    execute (request_type request);

    // Requests.
    //
  public:
    asio::awaitable<repository_type>
    get_repository (const std::string& owner, const std::string& repo);

    asio::awaitable<std::vector<release_type>>
    get_releases (const std::string& owner,
                  const std::string& repo,
                  std::optional<std::uint32_t> per_page = std::nullopt);

    // Note that the latest release is never a draft or a pre-release.
    //
    asio::awaitable<release_type>
    get_latest_release (const std::string& owner,
                        const std::string& repo);

    asio::awaitable<release_type>
    get_release_by_tag (const std::string& owner,
                        const std::string& repo,
                        const std::string& tag);

    asio::awaitable<release_type>
    get_release_by_id (const std::string& owner,
                       const std::string& repo,
                       std::uint64_t id);

    asio::awaitable<std::vector<commit_type>>
    get_commits (const std::string& owner,
                 const std::string& repo,
                 std::optional<std::uint32_t> per_page = std::nullopt);

    asio::awaitable<commit_type>
    get_commit (const std::string& owner,
                const std::string& repo,
                const std::string& sha);

    asio::awaitable<std::vector<branch_type>>
    get_branches (const std::string& owner,
                  const std::string& repo);

    asio::awaitable<branch_type>
    get_branch (const std::string& owner,
                const std::string& repo,
                const std::string& branch);

    asio::awaitable<std::vector<tag_type>>
    get_tags (const std::string& owner,
              const std::string& repo);

    asio::awaitable<std::vector<issue_type>>
    get_issues (const std::string& owner,
                const std::string& repo,
                std::optional<std::string> state = std::nullopt);

    asio::awaitable<issue_type>
    get_issue (const std::string& owner,
               const std::string& repo,
               std::uint64_t number);

    asio::awaitable<user_type>
    get_user (const std::string& username);

    asio::awaitable<user_type>
    get_authenticated_user ();

  private:
    asio::io_context& ioc_;
    http_client_traits traits_;
    std::optional<http_client> client_;
    std::optional<std::string> token_;
    std::optional<github_rate_limit> last_rate_limit_;
    progress_callback_type progress_callback_;

    // Return the HTTP client, creating it if necessary.
    //
    http_client&
    ensure_client ();

    response_type
    to_github_response (const http_response& r) const;

    // Add the headers that GitHub expects, unless already present.
    //
    void
    add_default_headers (http_request& req) const;

    // Return the rate limit if the headers contain any of the x-ratelimit-*
    // headers.
    //
    static std::optional<github_rate_limit>
    extract_rate_limit (const std::map<std::string, std::string>& headers);

    // Wait until the end of the window if the rate limit is exceeded.
    //
    asio::awaitable<void>
    handle_rate_limit (const github_rate_limit&);
  };
}
