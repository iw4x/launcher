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
    // Figure out how many seconds are left until the reset epoch. We use the
    // system clock since the GitHub API returns absolute UNIX timestamps for
    // rate limit resets.
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
    // Anything in the 2xx range is considered a successful HTTP response.
    //
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
    // 403 Forbidden is often used by GitHub for standard rate limits, while 429
    // Too Many Requests is used for secondary limits.
    //
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

    // Map user fields if we got a valid object. We verify presence first
    // because GitHub API responses can sometimes omit fields depending on
    // context (e.g., partial user objects in commit authors).
    //
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

    // Map the JSON repository structure. Note that some fields like description
    // can be explicitly null in the GitHub API response, so we must check for
    // that before casting to a string.
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

    // Parse main release details. Similar to repository descriptions, the name
    // and body fields might be null for untagged or empty releases.
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

      // Also grab embedded assets if present. A release doesn't strictly need
      // to have assets (e.g., tag-only releases).
      //
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

      // Dig into the nested commit object. The top-level author is the GitHub
      // account, but the nested commit.author is the raw git author data. We
      // try to stitch them together logically here.
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

      // Pull out label names. We just flatten this into an array of strings
      // rather than maintaining the full label object since we usually just
      // care about the tag text.
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
    : ioc_ (c),
      ssl_ctx_ (ssl::context::tlsv12_client)
  {
    // Configure default TLS settings. We require at least TLS 1.2.
    //
    ssl_ctx_.set_default_verify_paths ();
    ssl_ctx_.set_verify_mode (ssl::verify_none);
    ssl_ctx_.set_options (ssl::context::default_workarounds |
                          ssl::context::no_sslv2 |
                          ssl::context::no_sslv3 |
                          ssl::context::single_dh_use);
  }

  github_api::
  github_api (asio::io_context& c, string tok)
    : ioc_ (c),
      ssl_ctx_ (ssl::context::tlsv12_client),
      token_ (move (tok))
  {
    // Apply the same TLS strictness for authenticated instances.
    //
    ssl_ctx_.set_default_verify_paths ();
    ssl_ctx_.set_verify_mode (ssl::verify_none);
    ssl_ctx_.set_options (ssl::context::default_workarounds |
                          ssl::context::no_sslv2 |
                          ssl::context::no_sslv3 |
                          ssl::context::single_dh_use);
  }

  void github_api::
  set_token (string tok)
  {
    token_ = move (tok);
  }

  void github_api::
  set_proxy (string u)
  {
    proxy_url_ = move (u);
  }

  void github_api::
  set_progress_callback (progress_callback_type cb)
  {
    progress_callback_ = move (cb);
  }

  void github_api::
  add_default_headers (map<string, string>& hdrs) const
  {
    // Inject required GitHub API headers if they are missing. The API requires
    // a User-Agent, and the version header is recommended to prevent breakages
    // on API updates.
    //
    if (hdrs.find ("User-Agent") == hdrs.end ())
      hdrs["User-Agent"] = traits_type::user_agent ();

    if (hdrs.find ("Accept") == hdrs.end ())
      hdrs["Accept"] = "application/vnd.github+json";

    if (hdrs.find ("X-GitHub-Api-Version") == hdrs.end ())
      hdrs["X-GitHub-Api-Version"] = traits_type::api_version ();

    // Automatically append the bearer token if we have one globally set.
    //
    if (token_ && hdrs.find ("Authorization") == hdrs.end ())
      hdrs["Authorization"] = "Bearer " + *token_;
  }

  asio::awaitable<github_api::response_type> github_api::
  execute (request_type req)
  {
    // Bail or wait upfront if we already know our rate limit is completely
    // exhausted from a prior request. No sense in hammering the server just to
    // get a 403.
    //
    if (last_rate_limit_ && last_rate_limit_->is_exceeded ())
      co_await handle_rate_limit (*last_rate_limit_);

    string u (req.url ());
    string hst (endpoint_type::api_host);
    string tgt (u);

    // Strip the base prefix if the caller passed a fully qualified URL instead
    // of just the endpoint path. We just need the target path for the HTTP
    // request itself.
    //
    const string pre (endpoint_type::api_base);
    if (tgt.find (pre) == 0)
      tgt = tgt.substr (pre.length ());

    map<string, string> hdrs (req.headers);
    add_default_headers (hdrs);

    // Allow request-specific tokens to override the global one.
    //
    if (req.token)
      hdrs["Authorization"] = "Bearer " + *req.token;

    // Map our generic enum to the ASIO HTTP verb.
    //
    http::verb v (http::verb::get);
    switch (req.method)
    {
      case request_type::method_type::get:     v = http::verb::get;     break;
      case request_type::method_type::post:    v = http::verb::post;    break;
      case request_type::method_type::put:     v = http::verb::put;     break;
      case request_type::method_type::patch:   v = http::verb::patch;   break;
      case request_type::method_type::delete_: v = http::verb::delete_; break;
    }

    response_type res (co_await perform_request (hst, tgt, v, hdrs, req.body));

    // See if we got slapped with a rate limit mid-flight. If we did, we parse
    // the waiting period and retry automatically.
    //
    if (res.is_rate_limited ())
    {
      if (res.rate_limit && res.rate_limit->is_exceeded ())
      {
        co_await handle_rate_limit (*res.rate_limit);
      }
      else
      {
        // If we hit a secondary limit (429) or the standard headers are weirdly
        // absent, we fall back to checking 'retry-after' or default to a safe
        // 60-second backoff.
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

      // We waited. Try the exact same request one more time.
      //
      res = co_await perform_request (hst, tgt, v, hdrs, req.body);
    }

    co_return res;
  }

  asio::awaitable<github_api::response_type>
  github_api::
  perform_request (const string& hst,
                   const string& tgt,
                   http::verb mtd,
                   const map<string, string>& hdrs,
                   const optional<string>& bdy)
  {
    using tcp = asio::ip::tcp;

    response_type res;

    try
    {
      asio::ssl::stream<beast::tcp_stream> s (ioc_, ssl_ctx_);

      // Set the SNI (Server Name Indication) hostname. Many modern TLS
      // endpoints, including GitHub's load balancers, require this to route
      // requests and serve the correct certificate.
      //
      if (!SSL_set_tlsext_host_name (s.native_handle (), hst.c_str ()))
      {
        beast::error_code ec {static_cast<int> (::ERR_get_error ()),
                              asio::error::get_ssl_category ()};
        throw beast::system_error {ec};
      }

      auto& layer (beast::get_lowest_layer (s));

      if (proxy_url_ && !proxy_url_->empty ())
      {
        // Tunnel through the proxy to the destination, then TLS-handshake.
        //
        string pu (*proxy_url_);
        string pscheme, ph, pp;

        // Parse the proxy URL: extract scheme, strip userinfo, get host:port.
        //
        {
          size_t pos (0);
          size_t si (pu.find ("://", pos));
          if (si != string::npos)
          {
            pscheme = pu.substr (0, si);
            pos = si + 3;
          }
          else
            pscheme = "http";

          // Strip userinfo (user:pass@) if present.
          //
          size_t at (pu.find ('@', pos));
          size_t sl (pu.find ('/', pos));
          size_t auth_start (pos);

          if (at != string::npos && (sl == string::npos || at < sl))
            auth_start = at + 1;

          size_t ep (sl == string::npos ? pu.size () : sl);

          string authority (pu.substr (auth_start, ep - auth_start));
          size_t ci (authority.find (':'));

          if (ci != string::npos)
          {
            ph = authority.substr (0, ci);
            pp = authority.substr (ci + 1);
          }
          else
          {
            ph = authority;
            pp = (pscheme == "socks5" || pscheme == "socks5h" ||
                  pscheme == "socks4" || pscheme == "socks4a")
                  ? "1080" : "8080";
          }
        }

        bool socks (pscheme == "socks5"  || pscheme == "socks5h" ||
                    pscheme == "socks4a" || pscheme == "socks4");

        tcp::resolver pr (ioc_);
        auto peps (co_await pr.async_resolve (
            ph, pp, asio::use_awaitable));

        layer.expires_after (chrono::seconds (30));
        co_await layer.async_connect (peps, asio::use_awaitable);

        if (socks)
        {
          // SOCKS5 handshake (RFC 1928 / RFC 1929).
          //
          // Parse optional username/password from the proxy URL.
          //
          string suser, spass;
          {
            size_t pos (0);
            size_t si (pu.find ("://", pos));
            if (si != string::npos) pos = si + 3;

            size_t at (pu.find ('@', pos));
            if (at != string::npos)
            {
              size_t sl (pu.find ('/', pos));
              if (sl == string::npos || at < sl)
              {
                string info (pu.substr (pos, at - pos));
                size_t ci (info.find (':'));
                if (ci != string::npos)
                {
                  suser = info.substr (0, ci);
                  spass = info.substr (ci + 1);
                }
                else
                  suser = info;
              }
            }
          }

          bool has_auth (!suser.empty ());

          // Client greeting.
          //
          uint8_t greeting[4];
          greeting[0] = 0x05;
          if (has_auth)
          {
            greeting[1] = 0x02;
            greeting[2] = 0x00;
            greeting[3] = 0x02;
          }
          else
          {
            greeting[1] = 0x01;
            greeting[2] = 0x00;
          }

          size_t glen (has_auth ? 4 : 3);
          co_await asio::async_write (
            layer, asio::buffer (greeting, glen), asio::use_awaitable);

          uint8_t choice[2];
          co_await asio::async_read (
            layer, asio::buffer (choice, 2), asio::use_awaitable);

          if (choice[0] != 0x05)
            throw runtime_error ("SOCKS5 proxy returned invalid version");

          if (choice[1] == 0xFF)
            throw runtime_error (
              "SOCKS5 proxy: no acceptable authentication method");

          if (choice[1] == 0x02)
          {
            if (!has_auth)
              throw runtime_error (
                "SOCKS5 proxy requires authentication but no credentials");

            vector<uint8_t> auth;
            auth.reserve (3 + suser.size () + spass.size ());
            auth.push_back (0x01);
            auth.push_back (static_cast<uint8_t> (suser.size ()));
            auth.insert (auth.end (), suser.begin (), suser.end ());
            auth.push_back (static_cast<uint8_t> (spass.size ()));
            auth.insert (auth.end (), spass.begin (), spass.end ());

            co_await asio::async_write (
              layer, asio::buffer (auth), asio::use_awaitable);

            uint8_t aresp[2];
            co_await asio::async_read (
              layer, asio::buffer (aresp, 2), asio::use_awaitable);

            if (aresp[1] != 0x00)
              throw runtime_error ("SOCKS5 proxy authentication failed");
          }

          // CONNECT request: domain name (ATYP 0x03).
          //
          uint16_t dport (443);
          uint8_t port_hi (static_cast<uint8_t> (dport >> 8));
          uint8_t port_lo (static_cast<uint8_t> (dport & 0xFF));

          vector<uint8_t> creq;
          creq.reserve (7 + hst.size ());
          creq.push_back (0x05);
          creq.push_back (0x01);
          creq.push_back (0x00);
          creq.push_back (0x03);
          creq.push_back (static_cast<uint8_t> (hst.size ()));
          creq.insert (creq.end (), hst.begin (), hst.end ());
          creq.push_back (port_hi);
          creq.push_back (port_lo);

          co_await asio::async_write (
            layer, asio::buffer (creq), asio::use_awaitable);

          // Read reply header.
          //
          uint8_t hdr[4];
          co_await asio::async_read (
            layer, asio::buffer (hdr, 4), asio::use_awaitable);

          if (hdr[0] != 0x05)
            throw runtime_error (
              "SOCKS5 proxy returned invalid version in reply");

          if (hdr[1] != 0x00)
          {
            static const char* const errs[] = {
              "succeeded",
              "general SOCKS server failure",
              "connection not allowed by ruleset",
              "network unreachable",
              "host unreachable",
              "connection refused",
              "TTL expired",
              "command not supported",
              "address type not supported"
            };

            const char* msg (hdr[1] < 9 ? errs[hdr[1]] : "unknown error");
            throw runtime_error (
              string ("SOCKS5 proxy connect failed: ") + msg);
          }

          // Drain bound address + port.
          //
          size_t drain (0);
          switch (hdr[3])
          {
            case 0x01: drain = 4 + 2; break;
            case 0x04: drain = 16 + 2; break;
            case 0x03:
            {
              uint8_t len;
              co_await asio::async_read (
                layer, asio::buffer (&len, 1), asio::use_awaitable);
              drain = len + 2;
              break;
            }
            default:
              throw runtime_error (
                "SOCKS5 proxy returned unknown address type");
          }

          vector<uint8_t> sink (drain);
          co_await asio::async_read (
            layer, asio::buffer (sink), asio::use_awaitable);
        }
        else
        {
          // HTTP proxy: send CONNECT to establish a tunnel.
          //
          string authority (hst + ":443");

          http::request<http::empty_body> creq (
            http::verb::connect, authority, 11);
          creq.set (http::field::host, authority);

          layer.expires_after (chrono::seconds (30));
          co_await http::async_write (
            beast::get_lowest_layer (s), creq, asio::use_awaitable);

          beast::flat_buffer cbuf;
          http::response<http::empty_body> cres;
          co_await http::async_read (
            beast::get_lowest_layer (s), cbuf, cres, asio::use_awaitable);

          if (cres.result () != http::status::ok)
            throw runtime_error (
              "proxy CONNECT failed: " + to_string (cres.result_int ()));
        }
      }
      else
      {
        // Direct connection to GitHub.
        //
        tcp::resolver r (ioc_);
        auto const eps (co_await r.async_resolve (
            hst, "443", asio::use_awaitable));

        layer.expires_after (chrono::seconds (30));
        co_await layer.async_connect (eps, asio::use_awaitable);
      }

      co_await s.async_handshake (ssl::stream_base::client, asio::use_awaitable);

      // Construct the HTTP request. We use HTTP/1.1 here (11) as it is
      // perfectly fine for our one-off REST needs.
      //
      http::request<http::string_body> req {mtd, tgt, 11};
      req.set (http::field::host, hst);

      for (const auto& [k, v] : hdrs)
        req.set (k, v);

      if (bdy)
      {
        req.body () = *bdy;
        req.prepare_payload ();
      }

      // Reset timeout before writing data to the stream.
      //
      beast::get_lowest_layer (s).expires_after (chrono::seconds (30));
      co_await http::async_write (s, req, asio::use_awaitable);

      // Read response back into a string body. Note that large asset downloads
      // should probably not use this path to avoid memory exhaustion, but for
      // JSON endpoints it's perfectly fine.
      //
      beast::flat_buffer buf;
      http::response<http::string_body> raw;

      co_await http::async_read (s, buf, raw, asio::use_awaitable);

      res.status_code = raw.result_int ();
      res.body = raw.body ();

      // Normalize header names to lowercase. This makes it easier to reliably
      // look up things like rate limit boundaries later without worrying about
      // case variations.
      //
      for (const auto& f : raw)
      {
        string n (f.name_string ());
        transform (n.begin (),
                   n.end (),
                   n.begin (),
                   [] (unsigned char c)
        {
          return tolower (c);
        });

        res.headers[move (n)] = string (f.value ());
      }

      // Keep track of our global limits for subsequent queries.
      //
      res.rate_limit = extract_rate_limit (res.headers);
      if (res.rate_limit)
        last_rate_limit_ = *res.rate_limit;

      // If the request failed, try to parse a meaningful error message from the
      // returned JSON payload. If we fail to parse it, fallback to simply
      // reporting the HTTP status code.
      //
      if (!res.success ())
      {
        try
        {
          json::value v (json::parse (res.body));
          if (v.is_object () && v.as_object ().contains ("message"))
            res.error_message = json::value_to<string> (v.as_object ().at ("message"));
        }
        catch (...)
        {
          res.error_message = "HTTP error: " + to_string (res.status_code);
        }
      }

      // Perform a graceful TLS shutdown. We ignore EOF or truncated stream
      // errors here as they are extremely common and generally harmless when
      // shutting down client sockets.
      //
      beast::get_lowest_layer (s).expires_after (chrono::seconds (30));
      beast::error_code ec;
      co_await s.async_shutdown (asio::redirect_error (asio::use_awaitable, ec));

      if (ec == asio::error::eof || ec == ssl::error::stream_truncated)
        ec = {};

      if (ec)
        throw beast::system_error {ec};
    }
    catch (const boost::system::system_error& e)
    {
      // Catch ASIO or Boost internal network failures.
      //
      res.status_code = 0;
      res.error_message = string (e.what ()) + " [" +
                          e.code ().category ().name () + ":" +
                          to_string (e.code ().value ()) + "]";
    }
    catch (const exception& e)
    {
      // Catch standard exceptions (like JSON parsing issues that might bubble
      // up, though we try to contain those).
      //
      res.status_code = 0;
      res.error_message = e.what ();
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

    // Try to pull all standard rate limit headers out of the response. GitHub
    // almost always provides these unless we hit a catastrophic edge case or a
    // secondary limit (429) that doesn't respect the typical payload format.
    //
    if (extract ("x-ratelimit-limit", l.limit))         f = true;
    if (extract ("x-ratelimit-remaining", l.remaining)) f = true;
    if (extract ("x-ratelimit-reset", l.reset))         f = true;
    if (extract ("x-ratelimit-used", l.used))           f = true;

    return f ? optional<github_rate_limit> (l) : nullopt;
  }

  asio::awaitable<void> github_api::
  handle_rate_limit (const github_rate_limit& l)
  {
    // Wait it out if we crossed the line. We pad the sleep with 1 extra
    // second to make sure we don't accidentally fire the request slightly
    // before the server clock ticks over the threshold.
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
