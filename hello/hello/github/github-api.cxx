#include <hello/github/github-api.hxx>

using namespace std;

namespace hello
{
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

  typename github_api_traits::asset_type github_api_traits::
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

  typename github_api_traits::release_type github_api_traits::
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

  typename github_api_traits::issue_type github_api_traits::
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

  vector<typename github_api_traits::branch_type> github_api_traits::
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

  vector<typename github_api_traits::tag_type> github_api_traits::
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

  vector<typename github_api_traits::issue_type> github_api_traits::
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

  template <typename T> github_api<T>::
  github_api (asio::io_context& ioc)
    : ioc_ (ioc),
      ssl_ctx_ (ssl::context::tlsv12_client)
  {
    // FIXME: SSL is currently disabled due to unresolved handshake errors. This
    // needs to be investigated and corrected before SSL can be re-enabled.
    //
    ssl_ctx_.set_default_verify_paths ();
    ssl_ctx_.set_verify_mode (ssl::verify_none);
    ssl_ctx_.set_options (ssl::context::default_workarounds |
                          ssl::context::no_sslv2 |
                          ssl::context::no_sslv3 |
                          ssl::context::single_dh_use);
  }

  template <typename T> github_api<T>::
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

  template <typename T> void github_api<T>::
  add_default_headers (map<string, string>& headers) const
  {
    if (headers.find ("User-Agent") == headers.end ())
      headers["User-Agent"] = traits_type::user_agent ();

    if (headers.find ("Accept") == headers.end ())
      headers["Accept"] = "application/vnd.github+json";

    if (headers.find ("X-GitHub-Api-Version") == headers.end ())
      headers["X-GitHub-Api-Version"] = traits_type::api_version ();

    // @@: Potentially not needed.
    //
    if (token_ && headers.find ("Authorization") == headers.end ())
      headers["Authorization"] = "Bearer " + *token_;
  }
}
