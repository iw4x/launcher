#pragma once

#include <string>
#include <optional>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>

#include <launcher/github/github-types.hxx>
#include <launcher/github/github-endpoint.hxx>
#include <launcher/github/github-request.hxx>

namespace launcher
{
  namespace asio = boost::asio;
  namespace beast = boost::beast;
  namespace http = beast::http;
  namespace ssl = asio::ssl;
  namespace json = boost::json;

  // GitHub API rate limit information.
  //
  struct github_rate_limit
  {
    std::uint32_t limit;       // Maximum number of requests per hour
    std::uint32_t remaining;   // Number of requests remaining
    std::uint64_t reset;       // Unix timestamp when the rate limit resets
    std::uint32_t used;        // Number of requests used

    github_rate_limit ()
      : limit (0), remaining (0), reset (0), used (0) {}

    bool
    is_exceeded () const {return remaining == 0;}

    // Calculate seconds until reset.
    //
    std::uint64_t
    seconds_until_reset () const
    {
      // First, let's grab the current time. We are using system_clock here
      // because the reset member is a wall-clock timestamp.
      //
      auto now (std::chrono::system_clock::now ());

      // Now convert this to seconds since the epoch.
      //
      auto now_sec (
        std::chrono::duration_cast<std::chrono::seconds> (
          now.time_since_epoch ()).count ());

      // The count comes back as a signed type, so we need to cast it to
      // uint64_t to match our reset variable.
      //
      auto n (static_cast<std::uint64_t> (now_sec));

      // Finally, check if we've passed the mark. If the reset time is still
      // in the future, return the difference. Otherwise, we are already
      // there, so return 0.
      //
      return reset > n ? reset - n : 0;
    }
  };

  // GitHub API response.
  //
  struct github_response
  {
    unsigned status_code;
    std::string body;
    std::map<std::string, std::string> headers;
    std::optional<std::string> error_message;
    std::optional<github_rate_limit> rate_limit;

    bool
    success () const {return status_code >= 200 && status_code < 300;}

    bool
    empty () const {return body.empty ();}

    bool
    is_rate_limited () const {return status_code == 403 || status_code == 429;}
  };

  // GitHub API traits for customization.
  //
  struct github_api_traits
  {
    using request_type = github_request;
    using response_type = github_response;
    using endpoint_type = github_endpoint;

    // Type traits for GitHub entities.
    //
    using user_type = github_user;
    using repository_type = github_repository;
    using release_type = github_release;
    using asset_type = github_asset;
    using commit_type = github_commit;
    using issue_type = github_issue;
    using branch_type = github_branch;
    using tag_type = github_tag;

    // Parse JSON response into typed object.
    //
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

    // Parse array responses.
    //
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

    // Serialize objects to JSON.
    //
    static json::value
    to_json (const user_type& u);

    static json::value
    to_json (const repository_type& r);

    static json::value
    to_json (const release_type& r);

    // Default User-Agent header.
    //
    static std::string
    user_agent ()
    {
      return "iw4x-launcher/1.1";
    }

    // API version header.
    //
    static std::string
    api_version ()
    {
      return "2022-11-28";
    }
  };

  // GitHub API client with async/coroutine support.
  //
  template <typename T = github_api_traits>
  class github_api
  {
  public:
    using traits_type = T;
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

    // Constructor.
    //
    explicit
    github_api (asio::io_context& ioc);

    github_api (asio::io_context& ioc, std::string token);

    github_api (const github_api&) = delete;
    github_api& operator= (const github_api&) = delete;

    // Set authentication token.
    //
    void
    set_token (std::string token) {token_ = std::move (token);}

    // Set progress callback for rate limit notifications.
    //
    // The callback will be invoked when rate limiting occurs, with parameters:
    // - message: Description of what's happening
    // - seconds_remaining: Time until rate limit reset (updated each second)
    //
    using progress_callback_type =
      std::function<void (const std::string& message, std::uint64_t seconds_remaining)>;

    void
    set_progress_callback (progress_callback_type callback)
    {
      progress_callback_ = std::move (callback);
    }

    // Execute generic request.
    //
    asio::awaitable<response_type>
    execute (request_type request);

    // Repository operations.
    //
    asio::awaitable<repository_type>
    get_repository (const std::string& owner, const std::string& repo);

    // Release operations.
    //
    asio::awaitable<std::vector<release_type>>
    get_releases (const std::string& owner,
                  const std::string& repo,
                  std::optional<std::uint32_t> per_page = std::nullopt);

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

    // Commit operations.
    //
    asio::awaitable<std::vector<commit_type>>
    get_commits (const std::string& owner,
                 const std::string& repo,
                 std::optional<std::uint32_t> per_page = std::nullopt);

    asio::awaitable<commit_type>
    get_commit (const std::string& owner,
                const std::string& repo,
                const std::string& sha);

    // Branch operations.
    //
    asio::awaitable<std::vector<branch_type>>
    get_branches (const std::string& owner,
                  const std::string& repo);

    asio::awaitable<branch_type>
    get_branch (const std::string& owner,
                const std::string& repo,
                const std::string& branch);

    // Tag operations.
    //
    asio::awaitable<std::vector<tag_type>>
    get_tags (const std::string& owner,
              const std::string& repo);

    // Issue operations.
    //
    asio::awaitable<std::vector<issue_type>>
    get_issues (const std::string& owner,
                const std::string& repo,
                std::optional<std::string> state = std::nullopt);

    asio::awaitable<issue_type>
    get_issue (const std::string& owner,
               const std::string& repo,
               std::uint64_t number);

    // User operations.
    //
    asio::awaitable<user_type>
    get_user (const std::string& username);

    asio::awaitable<user_type>
    get_authenticated_user ();

  private:
    asio::io_context& ioc_;
    ssl::context ssl_ctx_;
    std::optional<std::string> token_;
    std::optional<github_rate_limit> last_rate_limit_;
    progress_callback_type progress_callback_;

    // Internal HTTP operations.
    //
    asio::awaitable<response_type>
    perform_request (const std::string& host,
                     const std::string& target,
                     http::verb method,
                     const std::map<std::string, std::string>& headers,
                     const std::optional<std::string>& body = std::nullopt);

    void
    add_default_headers (std::map<std::string, std::string>& headers) const;

    // Extract rate limit information from response headers.
    //
    static std::optional<github_rate_limit>
    extract_rate_limit (const std::map<std::string, std::string>& headers);

    // Check and handle rate limiting.
    //
    asio::awaitable<void>
    handle_rate_limit (const github_rate_limit&);
  };
}

#include <launcher/github/github-api.txx>
