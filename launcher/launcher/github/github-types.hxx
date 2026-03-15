#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace launcher
{
  // GitHub REST API data types.
  //
  // These types represent the core GitHub entities returned by the API.
  // They follow the GitHub REST API v3 specification.
  //

  // GitHub user/organization.
  //
  struct github_user
  {
    std::string login;
    std::uint64_t id;
    std::string node_id;
    std::string avatar_url;
    std::string html_url;
    std::string type; // "User" or "Organization"

    github_user () = default;
    github_user (std::string l, std::uint64_t i);

    bool
    empty () const;
  };

  // GitHub repository.
  //
  struct github_repository
  {
    using user_type = github_user;

    std::uint64_t id;
    std::string node_id;
    std::string name;
    std::string full_name;
    user_type owner;
    bool private_repo;
    std::string html_url;
    std::string description;
    bool fork;
    std::string default_branch;

    github_repository () = default;
    github_repository (std::string n, std::string fn);

    bool
    empty () const;
  };

  // GitHub release asset.
  //
  struct github_asset
  {
    std::uint64_t id;
    std::string node_id;
    std::string name;
    std::string label;
    std::string content_type;
    std::string state; // "uploaded", "open"
    std::uint64_t size;
    std::uint64_t download_count;
    std::string browser_download_url;
    std::string url; // API URL

    github_asset () = default;
    github_asset (std::string n, std::string u, std::uint64_t s);

    bool
    empty () const;
  };

  // GitHub release.
  //
  struct github_release
  {
    using user_type = github_user;
    using asset_type = github_asset;

    std::uint64_t id;
    std::string node_id;
    std::string tag_name;
    std::string target_commitish;
    std::string name;
    std::string body; // Markdown description
    bool draft;
    bool prerelease;
    user_type author;
    std::vector<asset_type> assets;
    std::string html_url;
    std::string tarball_url;
    std::string zipball_url;

    github_release () = default;
    github_release (std::string t, std::string n);

    bool
    empty () const;

    // Find asset by name or pattern.
    //
    std::optional<asset_type>
    find_asset (const std::string& name) const;

    std::optional<asset_type>
    find_asset_regex (const std::string& pattern) const;
  };

  // GitHub commit.
  //
  struct github_commit
  {
    using user_type = github_user;

    std::string sha;
    std::string node_id;
    std::string message;
    user_type author;
    user_type committer;
    std::string html_url;

    github_commit () = default;
    github_commit (std::string s, std::string m);

    bool
    empty () const;
  };

  // GitHub issue/pull request.
  //
  struct github_issue
  {
    using user_type = github_user;

    std::uint64_t id;
    std::string node_id;
    std::uint64_t number;
    std::string title;
    std::string body;
    user_type user;
    std::string state; // "open", "closed"
    bool locked;
    std::vector<std::string> labels;
    std::string html_url;

    github_issue () = default;
    github_issue (std::uint64_t n, std::string t);

    bool
    empty () const;
  };

  // GitHub branch.
  //
  struct github_branch
  {
    using commit_type = github_commit;

    std::string name;
    commit_type commit;
    bool protected_branch;

    github_branch () = default;
    explicit github_branch (std::string n);

    bool
    empty () const;
  };

  // GitHub tag.
  //
  struct github_tag
  {
    using commit_type = github_commit;

    std::string name;
    commit_type commit;
    std::string zipball_url;
    std::string tarball_url;

    github_tag () = default;
    explicit github_tag (std::string n);

    bool
    empty () const;
  };

  // Equality operators.
  //
  bool operator== (const github_user& x, const github_user& y) noexcept;
  bool operator!= (const github_user& x, const github_user& y) noexcept;

  bool operator== (const github_asset& x, const github_asset& y) noexcept;
  bool operator!= (const github_asset& x, const github_asset& y) noexcept;

  bool operator== (const github_release& x, const github_release& y) noexcept;
  bool operator!= (const github_release& x, const github_release& y) noexcept;
}
