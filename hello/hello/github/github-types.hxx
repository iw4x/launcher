#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace hello
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

    github_user (std::string l, std::uint64_t i)
      : login (std::move (l)), id (i) {}

    bool
    empty () const {return login.empty ();}
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

    github_repository (std::string n, std::string fn)
      : name (std::move (n)), full_name (std::move (fn)) {}

    bool
    empty () const {return name.empty ();}
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

    github_asset (std::string n, std::string u, std::uint64_t s)
      : name (std::move (n)), browser_download_url (std::move (u)), size (s) {}

    bool
    empty () const {return name.empty ();}
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

    github_release (std::string t, std::string n)
      : tag_name (std::move (t)), name (std::move (n)) {}

    bool
    empty () const {return tag_name.empty ();}

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

    github_commit (std::string s, std::string m)
      : sha (std::move (s)), message (std::move (m)) {}

    bool
    empty () const {return sha.empty ();}
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

    github_issue (std::uint64_t n, std::string t)
      : number (n), title (std::move (t)) {}

    bool
    empty () const {return title.empty ();}
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

    explicit
    github_branch (std::string n)
      : name (std::move (n)) {}

    bool
    empty () const {return name.empty ();}
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

    explicit
    github_tag (std::string n)
      : name (std::move (n)) {}

    bool
    empty () const {return name.empty ();}
  };

  // Equality operators.
  //
  inline bool
  operator== (const github_user& x,
              const github_user& y) noexcept
  {
    return x.login == y.login && x.id == y.id;
  }

  inline bool
  operator!= (const github_user& x,
              const github_user& y) noexcept
  {
    return !(x == y);
  }

  inline bool
  operator== (const github_asset& x,
              const github_asset& y) noexcept
  {
    return x.id == y.id && x.name == y.name;
  }

  inline bool
  operator!= (const github_asset& x,
              const github_asset& y) noexcept
  {
    return !(x == y);
  }

  inline bool
  operator== (const github_release& x,
              const github_release& y) noexcept
  {
    return x.id == y.id && x.tag_name == y.tag_name;
  }

  inline bool
  operator!= (const github_release& x,
              const github_release& y) noexcept
  {
    return !(x == y);
  }
}
