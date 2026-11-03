// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <launcher/github/github-endpoint.hxx>

#include <sstream>

using namespace std;

namespace launcher
{
  namespace
  {
    // Return the API base URL followed by the arguments.
    //
    template <typename... A>
    string
    build_endpoint (A&&... a)
    {
      ostringstream os;
      os << github_endpoint::api_base;
      (os << ... << a);
      return os.str ();
    }
  }

  string github_endpoint::
  repo (const string& o, const string& r)
  {
    return build_endpoint ("/repos/", o, "/", r);
  }

  string github_endpoint::
  repo_releases (const string& o, const string& r)
  {
    return build_endpoint ("/repos/", o, "/", r, "/releases");
  }

  string github_endpoint::
  repo_release_latest (const string& o, const string& r)
  {
    return build_endpoint ("/repos/", o, "/", r, "/releases/latest");
  }

  string github_endpoint::
  repo_release_tag (const string& o, const string& r, const string& t)
  {
    return build_endpoint ("/repos/", o, "/", r, "/releases/tags/", t);
  }

  string github_endpoint::
  repo_release_id (const string& o, const string& r, uint64_t i)
  {
    return build_endpoint ("/repos/", o, "/", r, "/releases/", to_string (i));
  }

  string github_endpoint::
  repo_commits (const string& o, const string& r)
  {
    return build_endpoint ("/repos/", o, "/", r, "/commits");
  }

  string github_endpoint::
  repo_commit (const string& o, const string& r, const string& s)
  {
    return build_endpoint ("/repos/", o, "/", r, "/commits/", s);
  }

  string github_endpoint::
  repo_branches (const string& o, const string& r)
  {
    return build_endpoint ("/repos/", o, "/", r, "/branches");
  }

  string github_endpoint::
  repo_branch (const string& o, const string& r, const string& b)
  {
    return build_endpoint ("/repos/", o, "/", r, "/branches/", b);
  }

  string github_endpoint::
  repo_tags (const string& o, const string& r)
  {
    return build_endpoint ("/repos/", o, "/", r, "/tags");
  }

  string github_endpoint::
  repo_issues (const string& o, const string& r)
  {
    return build_endpoint ("/repos/", o, "/", r, "/issues");
  }

  string github_endpoint::
  repo_issue (const string& o, const string& r, uint64_t n)
  {
    return build_endpoint ("/repos/", o, "/", r, "/issues/", to_string (n));
  }

  string github_endpoint::
  user (const string& u)
  {
    return build_endpoint ("/users/", u);
  }

  string github_endpoint::
  user_repos (const string& u)
  {
    return build_endpoint ("/users/", u, "/repos");
  }

  string github_endpoint::
  org (const string& on)
  {
    return build_endpoint ("/orgs/", on);
  }

  string github_endpoint::
  org_repos (const string& on)
  {
    return build_endpoint ("/orgs/", on, "/repos");
  }

  string github_endpoint::
  user_authenticated ()
  {
    return build_endpoint ("/user");
  }

  string github_endpoint::
  rate_limit ()
  {
    return build_endpoint ("/rate_limit");
  }
}
