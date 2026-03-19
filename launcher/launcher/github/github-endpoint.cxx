#include <launcher/github/github-endpoint.hxx>

#include <sstream>

using namespace std;

namespace launcher
{
  namespace
  {
    // Variadic helper to stitch together the API URL components. We use an
    // ostringstream here since it handles type conversions, though we might
    // want to keep an eye on allocation overhead if this gets hot in a tight
    // loop.
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

  // Repository specific endpoints.
  //
  // We assume the owner and repo strings are already properly URL-encoded
  // before they get here to avoid bad requests.
  //

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
    // Grab the latest published release. Note that this skips pre-releases and
    // drafts, which is usually exactly what we want for normal fetching.
    //
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

  // Commits, branches, and tags.
  //

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

  // Issues.
  //

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

  // Users and organizations.
  //

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

  // Global or authenticated-user specific endpoints.
  //

  string github_endpoint::
  user_authenticated ()
  {
    // Resolves based on the currently provided authentication token.
    //
    return build_endpoint ("/user");
  }

  string github_endpoint::
  rate_limit ()
  {
    // Useful to poll before we do a heavy batch of requests to make sure we
    // don't get arbitrarily throttled by the GitHub API.
    //
    return build_endpoint ("/rate_limit");
  }
}
