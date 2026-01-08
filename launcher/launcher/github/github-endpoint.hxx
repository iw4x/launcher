#pragma once

#include <string>
#include <sstream>
#include <optional>
#include <cstdint>

namespace launcher
{
  // GitHub API endpoint builder.
  //
  // Constructs GitHub REST API endpoint URLs following the pattern:
  // https://api.github.com/<path>
  //
  class github_endpoint
  {
  public:
    // API base URL.
    //
    static constexpr const char* api_host = "api.github.com";
    static constexpr const char* api_base = "https://api.github.com";

    // Repository endpoints.
    //
    static std::string
    repo (const std::string& owner, const std::string& repo)
    {
      return build ("/repos/", owner, "/", repo);
    }

    static std::string
    repo_releases (const std::string& owner, const std::string& repo)
    {
      return build ("/repos/", owner, "/", repo, "/releases");
    }

    static std::string
    repo_release_latest (const std::string& owner, const std::string& repo)
    {
      return build ("/repos/", owner, "/", repo, "/releases/latest");
    }

    static std::string
    repo_release_tag (const std::string& owner,
                      const std::string& repo,
                      const std::string& tag)
    {
      return build ("/repos/", owner, "/", repo, "/releases/tags/", tag);
    }

    static std::string
    repo_release_id (const std::string& owner,
                     const std::string& repo,
                     std::uint64_t id)
    {
      return build ("/repos/", owner, "/", repo, "/releases/", std::to_string (id));
    }

    static std::string
    repo_commits (const std::string& owner, const std::string& repo)
    {
      return build ("/repos/", owner, "/", repo, "/commits");
    }

    static std::string
    repo_commit (const std::string& owner,
                 const std::string& repo,
                 const std::string& sha)
    {
      return build ("/repos/", owner, "/", repo, "/commits/", sha);
    }

    static std::string
    repo_branches (const std::string& owner, const std::string& repo)
    {
      return build ("/repos/", owner, "/", repo, "/branches");
    }

    static std::string
    repo_branch (const std::string& owner,
                 const std::string& repo,
                 const std::string& branch)
    {
      return build ("/repos/", owner, "/", repo, "/branches/", branch);
    }

    static std::string
    repo_tags (const std::string& owner, const std::string& repo)
    {
      return build ("/repos/", owner, "/", repo, "/tags");
    }

    static std::string
    repo_issues (const std::string& owner, const std::string& repo)
    {
      return build ("/repos/", owner, "/", repo, "/issues");
    }

    static std::string
    repo_issue (const std::string& owner,
                const std::string& repo,
                std::uint64_t number)
    {
      return build ("/repos/", owner, "/", repo, "/issues/", std::to_string (number));
    }

    // User endpoints.
    //
    static std::string
    user (const std::string& username)
    {
      return build ("/users/", username);
    }

    static std::string
    user_repos (const std::string& username)
    {
      return build ("/users/", username, "/repos");
    }

    // Organization endpoints.
    //
    static std::string
    org (const std::string& org)
    {
      return build ("/orgs/", org);
    }

    static std::string
    org_repos (const std::string& org)
    {
      return build ("/orgs/", org, "/repos");
    }

    // Authenticated user endpoints.
    //
    static std::string
    user_authenticated ()
    {
      return build ("/user");
    }

    // Rate limit endpoint.
    //
    static std::string
    rate_limit ()
    {
      return build ("/rate_limit");
    }

  private:
    template <typename... Args>
    static std::string
    build (Args&&... args)
    {
      std::ostringstream os;
      os << api_base;
      (os << ... << args);
      return os.str ();
    }
  };
}
