#pragma once

#include <string>
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
    repo (const std::string& owner, const std::string& repo);

    static std::string
    repo_releases (const std::string& owner, const std::string& repo);

    static std::string
    repo_release_latest (const std::string& owner, const std::string& repo);

    static std::string
    repo_release_tag (const std::string& owner,
                      const std::string& repo,
                      const std::string& tag);

    static std::string
    repo_release_id (const std::string& owner,
                     const std::string& repo,
                     std::uint64_t id);

    static std::string
    repo_commits (const std::string& owner, const std::string& repo);

    static std::string
    repo_commit (const std::string& owner,
                 const std::string& repo,
                 const std::string& sha);

    static std::string
    repo_branches (const std::string& owner, const std::string& repo);

    static std::string
    repo_branch (const std::string& owner,
                 const std::string& repo,
                 const std::string& branch);

    static std::string
    repo_tags (const std::string& owner, const std::string& repo);

    static std::string
    repo_issues (const std::string& owner, const std::string& repo);

    static std::string
    repo_issue (const std::string& owner,
                const std::string& repo,
                std::uint64_t number);

    // User endpoints.
    //
    static std::string
    user (const std::string& username);

    static std::string
    user_repos (const std::string& username);

    // Organization endpoints.
    //
    static std::string
    org (const std::string& org);

    static std::string
    org_repos (const std::string& org);

    // Authenticated user endpoints.
    //
    static std::string
    user_authenticated ();

    // Rate limit endpoint.
    //
    static std::string
    rate_limit ();
  };
}
