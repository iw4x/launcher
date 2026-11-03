// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace launcher
{
  // Entities of the GitHub REST API, with the subset of the JSON fields that
  // we use. Note that the parsers (see github_api_traits) only set the
  // fields present in the JSON object.
  //
  // Each entity is empty if its identifying field is empty.
  //

  // User or organization.
  //
  struct github_user
  {
    std::string login;
    std::uint64_t id;
    std::string node_id;
    std::string avatar_url;
    std::string html_url;
    std::string type; // User or Organization.

    github_user () = default;
    github_user (std::string l, std::uint64_t i);

    bool
    empty () const;
  };

  // Repository.
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

  // Release asset.
  //
  struct github_asset
  {
    std::uint64_t id;
    std::string node_id;
    std::string name;
    std::string label;
    std::string content_type;
    std::string state; // uploaded or open.
    std::uint64_t size;
    std::uint64_t download_count;
    std::string browser_download_url;
    std::string url; // API URL, see browser_download_url for the file.

    github_asset () = default;
    github_asset (std::string n, std::string u, std::uint64_t s);

    bool
    empty () const;
  };

  // Release.
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
    std::string body; // Description in Markdown.
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

    // Return the first asset with this name, if any.
    //
    std::optional<asset_type>
    find_asset (const std::string& name) const;

    // Return the first asset whose entire name matches this ECMAScript
    // regular expression, if any. Throw std::regex_error if the pattern is
    // invalid.
    //
    std::optional<asset_type>
    find_asset_regex (const std::string& pattern) const;
  };

  // Commit.
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

  // Issue or pull request.
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
    std::string state; // open or closed.
    bool locked;
    std::vector<std::string> labels;
    std::string html_url;

    github_issue () = default;
    github_issue (std::uint64_t n, std::string t);

    bool
    empty () const;
  };

  // Branch.
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

  // Tag.
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

  // Note that the entities compare equal if their ids and their names
  // (login for a user, tag for a release) are equal.
  //
  bool operator== (const github_user& x, const github_user& y) noexcept;

  bool operator== (const github_asset& x, const github_asset& y) noexcept;

  bool operator== (const github_release& x, const github_release& y) noexcept;
}
