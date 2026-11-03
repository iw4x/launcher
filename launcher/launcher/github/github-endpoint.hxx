// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <cstdint>

namespace launcher
{
  // GitHub REST API endpoint URLs, for example:
  //
  // https://api.github.com/repos/<owner>/<repo>/releases
  //
  // Note that the arguments are used as is, without encoding.
  //
  class github_endpoint
  {
  public:
    static constexpr const char* api_host = "api.github.com";
    static constexpr const char* api_base = "https://api.github.com";

    // Repository.
    //
    static std::string
    repo (const std::string& owner, const std::string& repo);

    static std::string
    repo_releases (const std::string& owner, const std::string& repo);

    // Note that the latest release is never a draft or a pre-release.
    //
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

    // User and organization.
    //
    static std::string
    user (const std::string& username);

    static std::string
    user_repos (const std::string& username);

    static std::string
    org (const std::string& org);

    static std::string
    org_repos (const std::string& org);

    // User that the request token belongs to.
    //
    static std::string
    user_authenticated ();

    static std::string
    rate_limit ();
  };
}
