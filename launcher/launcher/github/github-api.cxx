#include <launcher/github/github-api.hxx>

#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <chrono>

using namespace std;

namespace launcher
{
  github_rate_limit::
  github_rate_limit ()
    : limit (0), remaining (0), reset (0), used (0)
  {
  }

  bool github_rate_limit::
  is_exceeded () const
  {
    return remaining == 0;
  }

  uint64_t github_rate_limit::
  seconds_until_reset () const
  {
    auto now (chrono::system_clock::now ());
    auto now_sec (
      chrono::duration_cast<chrono::seconds> (
        now.time_since_epoch ()).count ());
    auto n (static_cast<uint64_t> (now_sec));

    return reset > n ? reset - n : 0;
  }

  bool github_response::
  success () const
  {
    return status_code >= 200 && status_code < 300;
  }

  bool github_response::
  empty () const
  {
    return body.empty ();
  }

  bool github_response::
  is_rate_limited () const
  {
    return status_code == 403 || status_code == 429;
  }

  string github_api_traits::
  user_agent ()
  {
    return "iw4x-launcher/1.1";
  }

  string github_api_traits::
  api_version ()
  {
    return "2026-03-10";
  }

  github_api_traits::user_type github_api_traits::
  parse_user (const json::value& jv)
  {
    user_type u;

    if (jv.is_object ())
    {
      const auto& obj (jv.as_object ());

      if (obj.contains ("login"))
        u.login = json::value_to<string> (obj.at ("login"));

      if (obj.contains ("id"))
        u.id = json::value_to<uint64_t> (obj.at ("id"));

      if (obj.contains ("node_id"))
        u.node_id = json::value_to<string> (obj.at ("node_id"));

      if (obj.contains ("avatar_url"))
        u.avatar_url = json::value_to<string> (obj.at ("avatar_url"));

      if (obj.contains ("html_url"))
        u.html_url = json::value_to<string> (obj.at ("html_url"));

      if (obj.contains ("type"))
        u.type = json::value_to<string> (obj.at ("type"));
    }

    return u;
  }

  github_api_traits::repository_type github_api_traits::
  parse_repository (const json::value& jv)
  {
    repository_type r;

    if (jv.is_object ())
    {
      const auto& obj (jv.as_object ());

      if (obj.contains ("id"))
        r.id = json::value_to<uint64_t> (obj.at ("id"));

      if (obj.contains ("node_id"))
        r.node_id = json::value_to<string> (obj.at ("node_id"));

      if (obj.contains ("name"))
        r.name = json::value_to<string> (obj.at ("name"));

      if (obj.contains ("full_name"))
        r.full_name = json::value_to<string> (obj.at ("full_name"));

      if (obj.contains ("owner"))
        r.owner = parse_user (obj.at ("owner"));

      if (obj.contains ("private"))
        r.private_repo = json::value_to<bool> (obj.at ("private"));

      if (obj.contains ("html_url"))
        r.html_url = json::value_to<string> (obj.at ("html_url"));

      if (obj.contains ("description") && !obj.at ("description").is_null ())
        r.description = json::value_to<string> (obj.at ("description"));

      if (obj.contains ("fork"))
        r.fork = json::value_to<bool> (obj.at ("fork"));

      if (obj.contains ("default_branch"))
        r.default_branch = json::value_to<string> (obj.at ("default_branch"));
    }

    return r;
  }

  github_api_traits::asset_type github_api_traits::
  parse_asset (const json::value& jv)
  {
    asset_type a;

    if (jv.is_object ())
    {
      const auto& obj (jv.as_object ());

      if (obj.contains ("id"))
        a.id = json::value_to<uint64_t> (obj.at ("id"));

      if (obj.contains ("node_id"))
        a.node_id = json::value_to<string> (obj.at ("node_id"));

      if (obj.contains ("name"))
        a.name = json::value_to<string> (obj.at ("name"));

      if (obj.contains ("label") && !obj.at ("label").is_null ())
        a.label = json::value_to<string> (obj.at ("label"));

      if (obj.contains ("content_type"))
        a.content_type = json::value_to<string> (obj.at ("content_type"));

      if (obj.contains ("state"))
        a.state = json::value_to<string> (obj.at ("state"));

      if (obj.contains ("size"))
        a.size = json::value_to<uint64_t> (obj.at ("size"));

      if (obj.contains ("download_count"))
        a.download_count = json::value_to<uint64_t> (obj.at ("download_count"));

      if (obj.contains ("browser_download_url"))
        a.browser_download_url = json::value_to<string> (obj.at ("browser_download_url"));

      if (obj.contains ("url"))
        a.url = json::value_to<string> (obj.at ("url"));
    }

    return a;
  }

  github_api_traits::release_type github_api_traits::
  parse_release (const json::value& jv)
  {
    release_type r;

    if (jv.is_object ())
    {
      const auto& obj (jv.as_object ());

      if (obj.contains ("id"))
        r.id = json::value_to<uint64_t> (obj.at ("id"));

      if (obj.contains ("node_id"))
        r.node_id = json::value_to<string> (obj.at ("node_id"));

      if (obj.contains ("tag_name"))
        r.tag_name = json::value_to<string> (obj.at ("tag_name"));

      if (obj.contains ("target_commitish"))
        r.target_commitish = json::value_to<string> (obj.at ("target_commitish"));

      if (obj.contains ("name") && !obj.at ("name").is_null ())
        r.name = json::value_to<string> (obj.at ("name"));

      if (obj.contains ("body") && !obj.at ("body").is_null ())
        r.body = json::value_to<string> (obj.at ("body"));

      if (obj.contains ("draft"))
        r.draft = json::value_to<bool> (obj.at ("draft"));

      if (obj.contains ("prerelease"))
        r.prerelease = json::value_to<bool> (obj.at ("prerelease"));

      if (obj.contains ("author"))
        r.author = parse_user (obj.at ("author"));

      if (obj.contains ("html_url"))
        r.html_url = json::value_to<string> (obj.at ("html_url"));

      if (obj.contains ("tarball_url") && !obj.at ("tarball_url").is_null ())
        r.tarball_url = json::value_to<string> (obj.at ("tarball_url"));

      if (obj.contains ("zipball_url") && !obj.at ("zipball_url").is_null ())
        r.zipball_url = json::value_to<string> (obj.at ("zipball_url"));

      if (obj.contains ("assets") && obj.at ("assets").is_array ())
      {
        for (const auto& asset_jv : obj.at ("assets").as_array ())
          r.assets.push_back (parse_asset (asset_jv));
      }
    }

    return r;
  }

  github_api_traits::commit_type github_api_traits::
  parse_commit (const json::value& jv)
  {
    commit_type c;

    if (jv.is_object ())
    {
      const auto& obj (jv.as_object ());

      if (obj.contains ("sha"))
        c.sha = json::value_to<string> (obj.at ("sha"));

      if (obj.contains ("node_id"))
        c.node_id = json::value_to<string> (obj.at ("node_id"));

      if (obj.contains ("html_url"))
        c.html_url = json::value_to<string> (obj.at ("html_url"));

      if (obj.contains ("commit") && obj.at ("commit").is_object ())
      {
        const auto& commit_obj (obj.at ("commit").as_object ());

        if (commit_obj.contains ("message"))
          c.message = json::value_to<string> (commit_obj.at ("message"));

        if (commit_obj.contains ("author") && commit_obj.at ("author").is_object ())
        {
          const auto& author_obj (commit_obj.at ("author").as_object ());
          if (author_obj.contains ("name"))
            c.author.login = json::value_to<string> (author_obj.at ("name"));
        }
      }

      if (obj.contains ("author") && !obj.at ("author").is_null ())
        c.author = parse_user (obj.at ("author"));

      if (obj.contains ("committer") && !obj.at ("committer").is_null ())
        c.committer = parse_user (obj.at ("committer"));
    }

    return c;
  }

  github_api_traits::issue_type github_api_traits::
  parse_issue (const json::value& jv)
  {
    issue_type i;

    if (jv.is_object ())
    {
      const auto& obj (jv.as_object ());

      if (obj.contains ("id"))
        i.id = json::value_to<uint64_t> (obj.at ("id"));

      if (obj.contains ("node_id"))
        i.node_id = json::value_to<string> (obj.at ("node_id"));

      if (obj.contains ("number"))
        i.number = json::value_to<uint64_t> (obj.at ("number"));

      if (obj.contains ("title"))
        i.title = json::value_to<string> (obj.at ("title"));

      if (obj.contains ("body") && !obj.at ("body").is_null ())
        i.body = json::value_to<string> (obj.at ("body"));

      if (obj.contains ("user"))
        i.user = parse_user (obj.at ("user"));

      if (obj.contains ("state"))
        i.state = json::value_to<string> (obj.at ("state"));

      if (obj.contains ("locked"))
        i.locked = json::value_to<bool> (obj.at ("locked"));

      if (obj.contains ("html_url"))
        i.html_url = json::value_to<string> (obj.at ("html_url"));

      if (obj.contains ("labels") && obj.at ("labels").is_array ())
      {
        for (const auto& label_jv : obj.at ("labels").as_array ())
        {
          if (label_jv.is_object () && label_jv.as_object ().contains ("name"))
            i.labels.push_back (json::value_to<string> (label_jv.as_object ().at ("name")));
        }
      }
    }

    return i;
  }

  github_api_traits::branch_type github_api_traits::
  parse_branch (const json::value& jv)
  {
    branch_type b;

    if (jv.is_object ())
    {
      const auto& obj (jv.as_object ());

      if (obj.contains ("name"))
        b.name = json::value_to<string> (obj.at ("name"));

      if (obj.contains ("commit"))
        b.commit = parse_commit (obj.at ("commit"));

      if (obj.contains ("protected"))
        b.protected_branch = json::value_to<bool> (obj.at ("protected"));
    }

    return b;
  }

  github_api_traits::tag_type github_api_traits::
  parse_tag (const json::value& jv)
  {
    tag_type t;

    if (jv.is_object ())
    {
      const auto& obj (jv.as_object ());

      if (obj.contains ("name"))
        t.name = json::value_to<string> (obj.at ("name"));

      if (obj.contains ("commit"))
        t.commit = parse_commit (obj.at ("commit"));

      if (obj.contains ("zipball_url"))
        t.zipball_url = json::value_to<string> (obj.at ("zipball_url"));

      if (obj.contains ("tarball_url"))
        t.tarball_url = json::value_to<string> (obj.at ("tarball_url"));
    }

    return t;
  }

  vector<github_api_traits::release_type> github_api_traits::
  parse_releases (const json::value& jv)
  {
    vector<release_type> releases;

    if (jv.is_array ())
    {
      for (const auto& release_jv : jv.as_array ())
        releases.push_back (parse_release (release_jv));
    }

    return releases;
  }

  vector<github_api_traits::commit_type> github_api_traits::
  parse_commits (const json::value& jv)
  {
    vector<commit_type> commits;

    if (jv.is_array ())
    {
      for (const auto& commit_jv : jv.as_array ())
        commits.push_back (parse_commit (commit_jv));
    }

    return commits;
  }

  vector<github_api_traits::branch_type> github_api_traits::
  parse_branches (const json::value& jv)
  {
    vector<branch_type> branches;

    if (jv.is_array ())
    {
      for (const auto& branch_jv : jv.as_array ())
        branches.push_back (parse_branch (branch_jv));
    }

    return branches;
  }

  vector<github_api_traits::tag_type> github_api_traits::
  parse_tags (const json::value& jv)
  {
    vector<tag_type> tags;

    if (jv.is_array ())
    {
      for (const auto& tag_jv : jv.as_array ())
        tags.push_back (parse_tag (tag_jv));
    }

    return tags;
  }

  vector<github_api_traits::issue_type> github_api_traits::
  parse_issues (const json::value& jv)
  {
    vector<issue_type> issues;

    if (jv.is_array ())
    {
      for (const auto& issue_jv : jv.as_array ())
        issues.push_back (parse_issue (issue_jv));
    }

    return issues;
  }

  json::value github_api_traits::
  to_json (const user_type& u)
  {
    json::object obj;
    obj["login"] = u.login;
    obj["id"] = u.id;
    if (!u.node_id.empty ())
      obj["node_id"] = u.node_id;
    if (!u.avatar_url.empty ())
      obj["avatar_url"] = u.avatar_url;
    if (!u.html_url.empty ())
      obj["html_url"] = u.html_url;
    if (!u.type.empty ())
      obj["type"] = u.type;
    return obj;
  }

  json::value github_api_traits::
  to_json (const repository_type& r)
  {
    json::object obj;
    obj["id"] = r.id;
    obj["name"] = r.name;
    obj["full_name"] = r.full_name;
    obj["private"] = r.private_repo;
    if (!r.html_url.empty ())
      obj["html_url"] = r.html_url;
    if (!r.description.empty ())
      obj["description"] = r.description;
    obj["fork"] = r.fork;
    if (!r.default_branch.empty ())
      obj["default_branch"] = r.default_branch;
    return obj;
  }

  json::value github_api_traits::
  to_json (const release_type& r)
  {
    json::object obj;
    obj["id"] = r.id;
    obj["tag_name"] = r.tag_name;
    if (!r.name.empty ())
      obj["name"] = r.name;
    if (!r.body.empty ())
      obj["body"] = r.body;
    obj["draft"] = r.draft;
    obj["prerelease"] = r.prerelease;
    return obj;
  }

  github_api::
  github_api (asio::io_context& ioc)
    : ioc_ (ioc),
      ssl_ctx_ (ssl::context::tlsv12_client)
  {
    ssl_ctx_.set_default_verify_paths ();
    ssl_ctx_.set_verify_mode (ssl::verify_none);
    ssl_ctx_.set_options (ssl::context::default_workarounds |
                          ssl::context::no_sslv2 |
                          ssl::context::no_sslv3 |
                          ssl::context::single_dh_use);
  }

  github_api::
  github_api (asio::io_context& ioc, string token)
    : ioc_ (ioc),
      ssl_ctx_ (ssl::context::tlsv12_client),
      token_ (move (token))
  {
    ssl_ctx_.set_default_verify_paths ();
    ssl_ctx_.set_verify_mode (ssl::verify_none);
    ssl_ctx_.set_options (ssl::context::default_workarounds |
                          ssl::context::no_sslv2 |
                          ssl::context::no_sslv3 |
                          ssl::context::single_dh_use);
  }

  void github_api::
  set_token (string token)
  {
    token_ = move (token);
  }

  void github_api::
  set_progress_callback (progress_callback_type callback)
  {
    progress_callback_ = move (callback);
  }

  void github_api::
  add_default_headers (map<string, string>& headers) const
  {
    if (headers.find ("User-Agent") == headers.end ())
      headers["User-Agent"] = traits_type::user_agent ();

    if (headers.find ("Accept") == headers.end ())
      headers["Accept"] = "application/vnd.github+json";

    if (headers.find ("X-GitHub-Api-Version") == headers.end ())
      headers["X-GitHub-Api-Version"] = traits_type::api_version ();

    if (token_ && headers.find ("Authorization") == headers.end ())
      headers["Authorization"] = "Bearer " + *token_;
  }

  asio::awaitable<github_api::response_type> github_api::
  execute (request_type request)
  {
    if (last_rate_limit_ && last_rate_limit_->is_exceeded ())
      co_await handle_rate_limit (*last_rate_limit_);

    string url (request.url ());
    string host (endpoint_type::api_host);
    string target (url);

    const string prefix (endpoint_type::api_base);
    if (target.find (prefix) == 0)
      target = target.substr (prefix.length ());

    map<string, string> headers (request.headers);
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

    if (resp.is_rate_limited ())
    {
      if (resp.rate_limit && resp.rate_limit->is_exceeded ())
      {
        co_await handle_rate_limit (*resp.rate_limit);
      }
      else
      {
        github_rate_limit rl;
        rl.remaining = 0;

        uint64_t d (60);
        auto i (resp.headers.find ("retry-after"));

        if (i != resp.headers.end ())
        {
          try
          {
            d = stoull (i->second);
          }
          catch (...)
          {
          }
        }

        auto t (chrono::system_clock::now ());
        auto ts (static_cast<uint64_t> (
                   chrono::duration_cast<chrono::seconds> (
                     t.time_since_epoch ()).count ()));

        rl.reset = ts + d;

        co_await handle_rate_limit (rl);
      }

      resp = co_await perform_request (host, target, verb, headers, request.body);
    }

    co_return resp;
  }

  asio::awaitable<github_api::response_type>
  github_api::
  perform_request (const string& host,
                   const string& target,
                   http::verb method,
                   const map<string, string>& headers,
                   const optional<string>& body)
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

      beast::get_lowest_layer (stream).expires_after (chrono::seconds (30));
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

      beast::get_lowest_layer (stream).expires_after (chrono::seconds (30));
      co_await http::async_write (stream, req, asio::use_awaitable);

      beast::flat_buffer buffer;
      http::response<http::string_body> res;

      co_await http::async_read (stream, buffer, res, asio::use_awaitable);

      resp.status_code = res.result_int ();
      resp.body = res.body ();

      for (const auto& field : res)
      {
        string n (field.name_string ());
        transform (n.begin (),
                   n.end (),
                   n.begin (),
                   [] (unsigned char c)
        {
          return tolower (c);
        });

        resp.headers[move (n)] = string (field.value ());
      }

      resp.rate_limit = extract_rate_limit (resp.headers);
      if (resp.rate_limit)
        last_rate_limit_ = *resp.rate_limit;

      if (!resp.success ())
      {
        try
        {
          json::value jv (json::parse (resp.body));
          if (jv.is_object () && jv.as_object ().contains ("message"))
            resp.error_message = json::value_to<string> (jv.as_object ().at ("message"));
        }
        catch (...)
        {
          resp.error_message = "HTTP error: " + to_string (resp.status_code);
        }
      }

      beast::get_lowest_layer (stream).expires_after (chrono::seconds (30));
      beast::error_code ec;
      co_await stream.async_shutdown (asio::redirect_error (asio::use_awaitable, ec));

      if (ec == asio::error::eof || ec == ssl::error::stream_truncated)
        ec = {};

      if (ec)
        throw beast::system_error {ec};
    }
    catch (const boost::system::system_error& e)
    {
      resp.status_code = 0;
      resp.error_message = string (e.what ()) + " [" +
                           e.code ().category ().name () + ":" +
                           to_string (e.code ().value ()) + "]";
    }
    catch (const exception& e)
    {
      resp.status_code = 0;
      resp.error_message = e.what ();
    }

    co_return resp;
  }

  asio::awaitable<github_api::repository_type>
  github_api::
  get_repository (const string& owner, const string& repo)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo (owner, repo));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get repository"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_repository (jv);
  }

  asio::awaitable<vector<github_api::release_type>>
  github_api::
  get_releases (const string& owner,
                const string& repo,
                optional<uint32_t> per_page)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_releases (owner, repo));

    if (per_page)
      req.with_per_page (*per_page);

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get releases"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_releases (jv);
  }

  asio::awaitable<github_api::release_type>
  github_api::
  get_latest_release (const string& owner, const string& repo)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_release_latest (owner, repo));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get latest release"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_release (jv);
  }

  asio::awaitable<github_api::release_type>
  github_api::
  get_release_by_tag (const string& owner,
                      const string& repo,
                      const string& tag)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_release_tag (owner, repo, tag));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get release by tag"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_release (jv);
  }

  asio::awaitable<github_api::release_type>
  github_api::
  get_release_by_id (const string& owner,
                     const string& repo,
                     uint64_t id)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_release_id (owner, repo, id));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get release by ID"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_release (jv);
  }

  asio::awaitable<vector<github_api::commit_type>>
  github_api::
  get_commits (const string& owner,
               const string& repo,
               optional<uint32_t> per_page)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_commits (owner, repo));

    if (per_page)
      req.with_per_page (*per_page);

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get commits"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_commits (jv);
  }

  asio::awaitable<github_api::commit_type>
  github_api::
  get_commit (const string& owner,
              const string& repo,
              const string& sha)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_commit (owner, repo, sha));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get commit"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_commit (jv);
  }

  asio::awaitable<vector<github_api::branch_type>>
  github_api::
  get_branches (const string& owner, const string& repo)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_branches (owner, repo));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get branches"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_branches (jv);
  }

  asio::awaitable<github_api::branch_type>
  github_api::
  get_branch (const string& owner,
              const string& repo,
              const string& branch)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_branch (owner, repo, branch));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get branch"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_branch (jv);
  }

  asio::awaitable<vector<github_api::tag_type>>
  github_api::
  get_tags (const string& owner, const string& repo)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_tags (owner, repo));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get tags"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_tags (jv);
  }

  asio::awaitable<vector<github_api::issue_type>>
  github_api::
  get_issues (const string& owner,
              const string& repo,
              optional<string> state)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_issues (owner, repo));

    if (state)
      req.with_state (*state);

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get issues"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_issues (jv);
  }

  asio::awaitable<github_api::issue_type>
  github_api::
  get_issue (const string& owner,
             const string& repo,
             uint64_t number)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_issue (owner, repo, number));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get issue"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_issue (jv);
  }

  asio::awaitable<github_api::user_type>
  github_api::
  get_user (const string& username)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::user (username));

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get user"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_user (jv);
  }

  asio::awaitable<github_api::user_type>
  github_api::
  get_authenticated_user ()
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::user_authenticated ());

    response_type resp (co_await execute (move (req)));

    if (!resp.success ())
      throw runtime_error (resp.error_message.value_or ("Unable to get authenticated user"));

    json::value jv (json::parse (resp.body));
    co_return traits_type::parse_user (jv);
  }

  optional<github_rate_limit> github_api::
  extract_rate_limit (const map<string, string>& headers)
  {
    github_rate_limit l;
    bool found (false);

    auto extract = [&headers] (const char* name, auto& field)
    {
      auto it (headers.find (name));
      if (it != headers.end ())
      {
        try
        {
          if constexpr (is_same_v<decay_t<decltype (field)>, uint64_t>)
            field = stoull (it->second);
          else
            field = stoul (it->second);

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

    return found ? optional<github_rate_limit> (l) : nullopt;
  }

  asio::awaitable<void> github_api::
  handle_rate_limit (const github_rate_limit& limit)
  {
    if (limit.is_exceeded ())
    {
      auto wait (limit.seconds_until_reset () + 1);
      asio::steady_timer timer (ioc_);

      if (progress_callback_)
      {
        progress_callback_ (
          "GitHub API rate limit exceeded. Waiting for reset...",
          wait);

        for (auto rem (wait); rem > 0; --rem)
        {
          timer.expires_after (chrono::seconds (1));
          co_await timer.async_wait (asio::use_awaitable);

          progress_callback_ (
            "GitHub API rate limit exceeded. Waiting for reset...",
            rem - 1);
        }
      }
      else
      {
        timer.expires_after (chrono::seconds (wait));
        co_await timer.async_wait (asio::use_awaitable);
      }
    }
  }
}
