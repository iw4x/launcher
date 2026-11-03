// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/github/github-api.hxx>

#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <chrono>

#include <launcher/http/http-client.hxx>
#include <launcher/http/http-request.hxx>
#include <launcher/http/http-response.hxx>
#include <launcher/http/http-types.hxx>

using namespace std;

namespace launcher
{
  github_rate_limit::
  github_rate_limit ()
    : limit (0),
      remaining (0),
      reset (0),
      used (0)
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
    // Note that the reset time is a UNIX time, so we need the system clock.
    //
    auto now (chrono::system_clock::now ());
    auto sec (
      chrono::duration_cast<chrono::seconds> (
        now.time_since_epoch ()).count ());
    auto n (static_cast<uint64_t> (sec));

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
  parse_user (const json::value& v)
  {
    user_type u;

    if (v.is_object ())
    {
      const auto& o (v.as_object ());

      if (o.contains ("login"))
        u.login = json::value_to<string> (o.at ("login"));

      if (o.contains ("id"))
        u.id = json::value_to<uint64_t> (o.at ("id"));

      if (o.contains ("node_id"))
        u.node_id = json::value_to<string> (o.at ("node_id"));

      if (o.contains ("avatar_url"))
        u.avatar_url = json::value_to<string> (o.at ("avatar_url"));

      if (o.contains ("html_url"))
        u.html_url = json::value_to<string> (o.at ("html_url"));

      if (o.contains ("type"))
        u.type = json::value_to<string> (o.at ("type"));
    }

    return u;
  }

  github_api_traits::repository_type github_api_traits::
  parse_repository (const json::value& v)
  {
    repository_type r;

    // Note that GitHub sends null for some fields, the description for
    // example, so check for that as well.
    //
    if (v.is_object ())
    {
      const auto& o (v.as_object ());

      if (o.contains ("id"))
        r.id = json::value_to<uint64_t> (o.at ("id"));

      if (o.contains ("node_id"))
        r.node_id = json::value_to<string> (o.at ("node_id"));

      if (o.contains ("name"))
        r.name = json::value_to<string> (o.at ("name"));

      if (o.contains ("full_name"))
        r.full_name = json::value_to<string> (o.at ("full_name"));

      if (o.contains ("owner"))
        r.owner = parse_user (o.at ("owner"));

      if (o.contains ("private"))
        r.private_repo = json::value_to<bool> (o.at ("private"));

      if (o.contains ("html_url"))
        r.html_url = json::value_to<string> (o.at ("html_url"));

      if (o.contains ("description") && !o.at ("description").is_null ())
        r.description = json::value_to<string> (o.at ("description"));

      if (o.contains ("fork"))
        r.fork = json::value_to<bool> (o.at ("fork"));

      if (o.contains ("default_branch"))
        r.default_branch = json::value_to<string> (o.at ("default_branch"));
    }

    return r;
  }

  github_api_traits::asset_type github_api_traits::
  parse_asset (const json::value& v)
  {
    asset_type a;

    if (v.is_object ())
    {
      const auto& o (v.as_object ());

      if (o.contains ("id"))
        a.id = json::value_to<uint64_t> (o.at ("id"));

      if (o.contains ("node_id"))
        a.node_id = json::value_to<string> (o.at ("node_id"));

      if (o.contains ("name"))
        a.name = json::value_to<string> (o.at ("name"));

      if (o.contains ("label") && !o.at ("label").is_null ())
        a.label = json::value_to<string> (o.at ("label"));

      if (o.contains ("content_type"))
        a.content_type = json::value_to<string> (o.at ("content_type"));

      if (o.contains ("state"))
        a.state = json::value_to<string> (o.at ("state"));

      if (o.contains ("size"))
        a.size = json::value_to<uint64_t> (o.at ("size"));

      if (o.contains ("download_count"))
        a.download_count = json::value_to<uint64_t> (o.at ("download_count"));

      if (o.contains ("browser_download_url"))
        a.browser_download_url = json::value_to<string> (o.at ("browser_download_url"));

      if (o.contains ("url"))
        a.url = json::value_to<string> (o.at ("url"));
    }

    return a;
  }

  github_api_traits::release_type github_api_traits::
  parse_release (const json::value& v)
  {
    release_type r;

    // As above.
    //
    if (v.is_object ())
    {
      const auto& o (v.as_object ());

      if (o.contains ("id"))
        r.id = json::value_to<uint64_t> (o.at ("id"));

      if (o.contains ("node_id"))
        r.node_id = json::value_to<string> (o.at ("node_id"));

      if (o.contains ("tag_name"))
        r.tag_name = json::value_to<string> (o.at ("tag_name"));

      if (o.contains ("target_commitish"))
        r.target_commitish = json::value_to<string> (o.at ("target_commitish"));

      if (o.contains ("name") && !o.at ("name").is_null ())
        r.name = json::value_to<string> (o.at ("name"));

      if (o.contains ("body") && !o.at ("body").is_null ())
        r.body = json::value_to<string> (o.at ("body"));

      if (o.contains ("draft"))
        r.draft = json::value_to<bool> (o.at ("draft"));

      if (o.contains ("prerelease"))
        r.prerelease = json::value_to<bool> (o.at ("prerelease"));

      if (o.contains ("author"))
        r.author = parse_user (o.at ("author"));

      if (o.contains ("html_url"))
        r.html_url = json::value_to<string> (o.at ("html_url"));

      if (o.contains ("tarball_url") && !o.at ("tarball_url").is_null ())
        r.tarball_url = json::value_to<string> (o.at ("tarball_url"));

      if (o.contains ("zipball_url") && !o.at ("zipball_url").is_null ())
        r.zipball_url = json::value_to<string> (o.at ("zipball_url"));

      if (o.contains ("assets") && o.at ("assets").is_array ())
      {
        for (const auto& a : o.at ("assets").as_array ())
          r.assets.push_back (parse_asset (a));
      }
    }

    return r;
  }

  github_api_traits::commit_type github_api_traits::
  parse_commit (const json::value& v)
  {
    commit_type c;

    if (v.is_object ())
    {
      const auto& o (v.as_object ());

      if (o.contains ("sha"))
        c.sha = json::value_to<string> (o.at ("sha"));

      if (o.contains ("node_id"))
        c.node_id = json::value_to<string> (o.at ("node_id"));

      if (o.contains ("html_url"))
        c.html_url = json::value_to<string> (o.at ("html_url"));

      // The top-level author is the GitHub account and commit.author is the
      // git author. Use the git author name as the login, which the account
      // below overrides if the author has one.
      //
      if (o.contains ("commit") && o.at ("commit").is_object ())
      {
        const auto& co (o.at ("commit").as_object ());

        if (co.contains ("message"))
          c.message = json::value_to<string> (co.at ("message"));

        if (co.contains ("author") && co.at ("author").is_object ())
        {
          const auto& ao (co.at ("author").as_object ());
          if (ao.contains ("name"))
            c.author.login = json::value_to<string> (ao.at ("name"));
        }
      }

      if (o.contains ("author") && !o.at ("author").is_null ())
        c.author = parse_user (o.at ("author"));

      if (o.contains ("committer") && !o.at ("committer").is_null ())
        c.committer = parse_user (o.at ("committer"));
    }

    return c;
  }

  github_api_traits::issue_type github_api_traits::
  parse_issue (const json::value& v)
  {
    issue_type i;

    if (v.is_object ())
    {
      const auto& o (v.as_object ());

      if (o.contains ("id"))
        i.id = json::value_to<uint64_t> (o.at ("id"));

      if (o.contains ("node_id"))
        i.node_id = json::value_to<string> (o.at ("node_id"));

      if (o.contains ("number"))
        i.number = json::value_to<uint64_t> (o.at ("number"));

      if (o.contains ("title"))
        i.title = json::value_to<string> (o.at ("title"));

      if (o.contains ("body") && !o.at ("body").is_null ())
        i.body = json::value_to<string> (o.at ("body"));

      if (o.contains ("user"))
        i.user = parse_user (o.at ("user"));

      if (o.contains ("state"))
        i.state = json::value_to<string> (o.at ("state"));

      if (o.contains ("locked"))
        i.locked = json::value_to<bool> (o.at ("locked"));

      if (o.contains ("html_url"))
        i.html_url = json::value_to<string> (o.at ("html_url"));

      // We only keep the label names.
      //
      if (o.contains ("labels") && o.at ("labels").is_array ())
      {
        for (const auto& l : o.at ("labels").as_array ())
        {
          if (l.is_object () && l.as_object ().contains ("name"))
            i.labels.push_back (json::value_to<string> (l.as_object ().at ("name")));
        }
      }
    }

    return i;
  }

  github_api_traits::branch_type github_api_traits::
  parse_branch (const json::value& v)
  {
    branch_type b;

    if (v.is_object ())
    {
      const auto& o (v.as_object ());

      if (o.contains ("name"))
        b.name = json::value_to<string> (o.at ("name"));

      if (o.contains ("commit"))
        b.commit = parse_commit (o.at ("commit"));

      if (o.contains ("protected"))
        b.protected_branch = json::value_to<bool> (o.at ("protected"));
    }

    return b;
  }

  github_api_traits::tag_type github_api_traits::
  parse_tag (const json::value& v)
  {
    tag_type t;

    if (v.is_object ())
    {
      const auto& o (v.as_object ());

      if (o.contains ("name"))
        t.name = json::value_to<string> (o.at ("name"));

      if (o.contains ("commit"))
        t.commit = parse_commit (o.at ("commit"));

      if (o.contains ("zipball_url"))
        t.zipball_url = json::value_to<string> (o.at ("zipball_url"));

      if (o.contains ("tarball_url"))
        t.tarball_url = json::value_to<string> (o.at ("tarball_url"));
    }

    return t;
  }

  vector<github_api_traits::release_type> github_api_traits::
  parse_releases (const json::value& v)
  {
    vector<release_type> rs;

    if (v.is_array ())
    {
      for (const auto& r : v.as_array ())
        rs.push_back (parse_release (r));
    }

    return rs;
  }

  vector<github_api_traits::commit_type> github_api_traits::
  parse_commits (const json::value& v)
  {
    vector<commit_type> cs;

    if (v.is_array ())
    {
      for (const auto& c : v.as_array ())
        cs.push_back (parse_commit (c));
    }

    return cs;
  }

  vector<github_api_traits::branch_type> github_api_traits::
  parse_branches (const json::value& v)
  {
    vector<branch_type> bs;

    if (v.is_array ())
    {
      for (const auto& b : v.as_array ())
        bs.push_back (parse_branch (b));
    }

    return bs;
  }

  vector<github_api_traits::tag_type> github_api_traits::
  parse_tags (const json::value& v)
  {
    vector<tag_type> ts;

    if (v.is_array ())
    {
      for (const auto& t : v.as_array ())
        ts.push_back (parse_tag (t));
    }

    return ts;
  }

  vector<github_api_traits::issue_type> github_api_traits::
  parse_issues (const json::value& v)
  {
    vector<issue_type> is;

    if (v.is_array ())
    {
      for (const auto& i : v.as_array ())
        is.push_back (parse_issue (i));
    }

    return is;
  }

  json::value github_api_traits::
  to_json (const user_type& u)
  {
    json::object o;
    o["login"] = u.login;
    o["id"] = u.id;

    if (!u.node_id.empty ())
      o["node_id"] = u.node_id;

    if (!u.avatar_url.empty ())
      o["avatar_url"] = u.avatar_url;

    if (!u.html_url.empty ())
      o["html_url"] = u.html_url;

    if (!u.type.empty ())
      o["type"] = u.type;

    return o;
  }

  json::value github_api_traits::
  to_json (const repository_type& r)
  {
    json::object o;
    o["id"] = r.id;
    o["name"] = r.name;
    o["full_name"] = r.full_name;
    o["private"] = r.private_repo;

    if (!r.html_url.empty ())
      o["html_url"] = r.html_url;

    if (!r.description.empty ())
      o["description"] = r.description;

    o["fork"] = r.fork;

    if (!r.default_branch.empty ())
      o["default_branch"] = r.default_branch;

    return o;
  }

  json::value github_api_traits::
  to_json (const release_type& r)
  {
    json::object o;
    o["id"] = r.id;
    o["tag_name"] = r.tag_name;

    if (!r.name.empty ())
      o["name"] = r.name;

    if (!r.body.empty ())
      o["body"] = r.body;

    o["draft"] = r.draft;
    o["prerelease"] = r.prerelease;

    return o;
  }

  github_api::
  github_api (asio::io_context& c)
    : ioc_ (c)
  {
    traits_.user_agent = traits_type::user_agent ();
  }

  github_api::
  github_api (asio::io_context& c, string tok)
    : ioc_ (c),
      token_ (move (tok))
  {
    traits_.user_agent = traits_type::user_agent ();
  }

  void github_api::
  set_token (string tok)
  {
    token_ = move (tok);
  }

  void github_api::
  set_proxy (string u)
  {
    traits_.proxy_url = move (u);

    // The client takes the proxy on construction, so recreate it.
    //
    client_.reset ();
  }

  void github_api::
  set_progress_callback (progress_callback_type cb)
  {
    progress_callback_ = move (cb);
  }

  http_client& github_api::
  ensure_client ()
  {
    if (!client_)
      client_.emplace (ioc_, traits_);

    return *client_;
  }

  void github_api::
  add_default_headers (http_request& req) const
  {
    if (!req.has_header ("User-Agent"))
      req.set_header ("User-Agent", traits_type::user_agent ());

    if (!req.has_header ("Accept"))
      req.set_header ("Accept", "application/vnd.github+json");

    if (!req.has_header ("X-GitHub-Api-Version"))
      req.set_header ("X-GitHub-Api-Version", traits_type::api_version ());

    if (token_ && !req.has_header ("Authorization"))
      req.set_bearer_token (*token_);
  }

  static http_method
  to_http_method (github_request::method_type m)
  {
    switch (m)
    {
      case github_request::method_type::get:     return http_method::get;
      case github_request::method_type::post:    return http_method::post;
      case github_request::method_type::put:     return http_method::put;
      case github_request::method_type::patch:   return http_method::patch;
      case github_request::method_type::delete_: return http_method::delete_;
    }

    return http_method::get;
  }

  github_api::response_type github_api::
  to_github_response (const http_response& r) const
  {
    response_type res;

    res.status_code = r.status_code ();
    res.body = r.body.value_or (string ());

    // Lower-case the header names so that we can look them up.
    //
    for (auto it (r.headers.begin ()); it != r.headers.end (); ++it)
    {
      string n (it->name);
      transform (n.begin (),
                 n.end (),
                 n.begin (),
                 [] (unsigned char c) { return tolower (c); });

      res.headers[move (n)] = it->value;
    }

    res.rate_limit = extract_rate_limit (res.headers);

    // On failure get the message from the JSON body or, if it is not JSON,
    // use the status code.
    //
    if (!res.success ())
    {
      try
      {
        json::value v (json::parse (res.body));
        if (v.is_object () && v.as_object ().contains ("message"))
          res.error_message =
            json::value_to<string> (v.as_object ().at ("message"));
      }
      catch (...)
      {
        res.error_message = "HTTP error: " + std::to_string (res.status_code);
      }
    }

    return res;
  }

  asio::awaitable<github_api::response_type> github_api::
  execute (request_type req)
  {
    // If the previous response exhausted the rate limit, then wait it out
    // instead of making a request that is bound to fail.
    //
    if (last_rate_limit_ && last_rate_limit_->is_exceeded ())
      co_await handle_rate_limit (*last_rate_limit_);

    // The endpoint is normally a full URL (see github_endpoint) but can also
    // be a path relative to the API base.
    //
    string u (req.url ());
    const string pre (endpoint_type::api_base);

    string full_url;
    if (u.find ("https://") == 0 || u.find ("http://") == 0)
      full_url = move (u);
    else
      full_url = pre + u;

    http_request hr (to_http_method (req.method), full_url);

    for (const auto& [k, v] : req.headers)
      hr.set_header (k, v);

    add_default_headers (hr);

    if (req.token)
      hr.set_bearer_token (*req.token);

    if (req.body)
      hr.set_body (*req.body);

    hr.normalize ();

    http_client& c (ensure_client ());

    response_type res;

    try
    {
      http_response raw (co_await c.request (hr));
      res = to_github_response (raw);

      if (res.rate_limit)
        last_rate_limit_ = *res.rate_limit;
    }
    catch (const exception& e)
    {
      res.status_code = 0;
      res.error_message = e.what ();
    }

    // If we are rate limited, wait it out and retry once.
    //
    // @@ A 403 for any other reason also ends up waiting.
    //
    if (res.is_rate_limited ())
    {
      if (res.rate_limit && res.rate_limit->is_exceeded ())
      {
        co_await handle_rate_limit (*res.rate_limit);
      }
      else
      {
        // For a secondary limit the headers don't show an exhausted limit,
        // so wait for the retry-after header seconds or a minute if there is
        // none.
        //
        github_rate_limit rl;
        rl.remaining = 0;

        uint64_t d (60);
        auto i (res.headers.find ("retry-after"));

        if (i != res.headers.end ())
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

      try
      {
        http_response raw (co_await c.request (hr));
        res = to_github_response (raw);

        if (res.rate_limit)
          last_rate_limit_ = *res.rate_limit;
      }
      catch (const exception& e)
      {
        res.status_code = 0;
        res.error_message = e.what ();
      }
    }

    co_return res;
  }

  asio::awaitable<github_api::repository_type>
  github_api::
  get_repository (const string& own, const string& rep)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo (own, rep));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (res.error_message.value_or ("Unable to get repository"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_repository (v);
  }

  asio::awaitable<vector<github_api::release_type>>
  github_api::
  get_releases (const string& own,
                const string& rep,
                optional<uint32_t> pp)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_releases (own, rep));

    if (pp)
      req.with_per_page (*pp);

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (res.error_message.value_or ("Unable to get releases"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_releases (v);
  }

  asio::awaitable<github_api::release_type>
  github_api::
  get_latest_release (const string& own, const string& rep)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_release_latest (own, rep));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (
        res.error_message.value_or ("Unable to get latest release"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_release (v);
  }

  asio::awaitable<github_api::release_type>
  github_api::get_release_by_tag (const string& own,
                                  const string& rep,
                                  const string& tag)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_release_tag (own, rep, tag));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (
        res.error_message.value_or ("Unable to get release by tag"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_release (v);
  }

  asio::awaitable<github_api::release_type>
  github_api::get_release_by_id (const string& own,
                                 const string& rep,
                                 uint64_t id)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_release_id (own, rep, id));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (
        res.error_message.value_or ("Unable to get release by ID"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_release (v);
  }

  asio::awaitable<vector<github_api::commit_type>>
  github_api::get_commits (const string& own,
                           const string& rep,
                           optional<uint32_t> pp)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_commits (own, rep));

    if (pp)
      req.with_per_page (*pp);

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (
        res.error_message.value_or ("Unable to get commits"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_commits (v);
  }

  asio::awaitable<github_api::commit_type>
  github_api::get_commit (const string& own,
                          const string& rep,
                          const string& sha)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_commit (own, rep, sha));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (res.error_message.value_or ("Unable to get commit"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_commit (v);
  }

  asio::awaitable<vector<github_api::branch_type>>
  github_api::get_branches (const string& own, const string& rep)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_branches (own, rep));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (
        res.error_message.value_or ("Unable to get branches"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_branches (v);
  }

  asio::awaitable<github_api::branch_type>
  github_api::get_branch (const string& own,
                          const string& rep,
                          const string& br)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_branch (own, rep, br));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (res.error_message.value_or ("Unable to get branch"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_branch (v);
  }

  asio::awaitable<vector<github_api::tag_type>>
  github_api::get_tags (const string& own, const string& rep)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_tags (own, rep));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (res.error_message.value_or ("Unable to get tags"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_tags (v);
  }

  asio::awaitable<vector<github_api::issue_type>>
  github_api::get_issues (const string& own,
                          const string& rep,
                          optional<string> st)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_issues (own, rep));

    if (st)
      req.with_state (*st);

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (res.error_message.value_or ("Unable to get issues"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_issues (v);
  }

  asio::awaitable<github_api::issue_type>
  github_api::get_issue (const string& own, const string& rep, uint64_t num)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::repo_issue (own, rep, num));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (res.error_message.value_or ("Unable to get issue"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_issue (v);
  }

  asio::awaitable<github_api::user_type>
  github_api::get_user (const string& usr)
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::user (usr));

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (res.error_message.value_or ("Unable to get user"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_user (v);
  }

  asio::awaitable<github_api::user_type>
  github_api::get_authenticated_user ()
  {
    request_type req (request_type::method_type::get,
                      endpoint_type::user_authenticated ());

    response_type res (co_await execute (move (req)));

    if (!res.success ())
      throw runtime_error (
        res.error_message.value_or ("Unable to get authenticated user"));

    json::value v (json::parse (res.body));
    co_return traits_type::parse_user (v);
  }

  optional<github_rate_limit> github_api::
  extract_rate_limit (const map<string, string>& hdrs)
  {
    github_rate_limit l;
    bool f (false);

    auto extract = [&hdrs] (const char* n, auto& fld)
    {
      auto i (hdrs.find (n));
      if (i != hdrs.end ())
      {
        try
        {
          if constexpr (is_same_v<decay_t<decltype (fld)>, uint64_t>)
            fld = stoull (i->second);
          else
            fld = stoul (i->second);

          return true;
        }
        catch (...) {}
      }
      return false;
    };

    if (extract ("x-ratelimit-limit", l.limit))         f = true;
    if (extract ("x-ratelimit-remaining", l.remaining)) f = true;
    if (extract ("x-ratelimit-reset", l.reset))         f = true;
    if (extract ("x-ratelimit-used", l.used))           f = true;

    return f ? optional<github_rate_limit> (l) : nullopt;
  }

  asio::awaitable<void> github_api::
  handle_rate_limit (const github_rate_limit& l)
  {
    // Wait an extra second in case our clock is ahead of the server's.
    //
    if (l.is_exceeded ())
    {
      auto w (l.seconds_until_reset () + 1);
      asio::steady_timer t (ioc_);

      if (progress_callback_)
      {
        progress_callback_ (
          "GitHub API rate limit exceeded. Waiting for reset...", w);

        for (auto r (w); r > 0; --r)
        {
          t.expires_after (chrono::seconds (1));
          co_await t.async_wait (asio::use_awaitable);

          progress_callback_ (
            "GitHub API rate limit exceeded. Waiting for reset...", r - 1);
        }
      }
      else
      {
        t.expires_after (chrono::seconds (w));
        co_await t.async_wait (asio::use_awaitable);
      }
    }
  }
}
