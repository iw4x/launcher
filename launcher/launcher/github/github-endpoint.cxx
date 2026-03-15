#include <launcher/github/github-endpoint.hxx>

#include <sstream>

using namespace std;

namespace launcher
{
  namespace
  {
    template <typename... Args>
    string
    build_endpoint (Args&&... args)
    {
      ostringstream os;
      os << github_endpoint::api_base;
      (os << ... << args);
      return os.str ();
    }
  }

  string github_endpoint::
  repo (const string& owner, const string& repo)
  {
    return build_endpoint ("/repos/", owner, "/", repo);
  }

  string github_endpoint::
  repo_releases (const string& owner, const string& repo)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/releases");
  }

  string github_endpoint::
  repo_release_latest (const string& owner, const string& repo)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/releases/latest");
  }

  string github_endpoint::
  repo_release_tag (const string& owner,
                    const string& repo,
                    const string& tag)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/releases/tags/", tag);
  }

  string github_endpoint::
  repo_release_id (const string& owner,
                   const string& repo,
                   uint64_t id)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/releases/", to_string (id));
  }

  string github_endpoint::
  repo_commits (const string& owner, const string& repo)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/commits");
  }

  string github_endpoint::
  repo_commit (const string& owner,
               const string& repo,
               const string& sha)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/commits/", sha);
  }

  string github_endpoint::
  repo_branches (const string& owner, const string& repo)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/branches");
  }

  string github_endpoint::
  repo_branch (const string& owner,
               const string& repo,
               const string& branch)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/branches/", branch);
  }

  string github_endpoint::
  repo_tags (const string& owner, const string& repo)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/tags");
  }

  string github_endpoint::
  repo_issues (const string& owner, const string& repo)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/issues");
  }

  string github_endpoint::
  repo_issue (const string& owner,
              const string& repo,
              uint64_t number)
  {
    return build_endpoint ("/repos/", owner, "/", repo, "/issues/", to_string (number));
  }

  string github_endpoint::
  user (const string& username)
  {
    return build_endpoint ("/users/", username);
  }

  string github_endpoint::
  user_repos (const string& username)
  {
    return build_endpoint ("/users/", username, "/repos");
  }

  string github_endpoint::
  org (const string& org_name)
  {
    return build_endpoint ("/orgs/", org_name);
  }

  string github_endpoint::
  org_repos (const string& org_name)
  {
    return build_endpoint ("/orgs/", org_name, "/repos");
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
