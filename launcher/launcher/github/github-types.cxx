#include <launcher/github/github-types.hxx>

#include <algorithm>
#include <regex>
#include <utility>

using namespace std;

namespace launcher
{
  github_user::
  github_user (string l, uint64_t i)
    : login (move (l)), id (i)
  {
  }

  bool github_user::
  empty () const
  {
    return login.empty ();
  }

  github_repository::
  github_repository (string n, string fn)
    : name (move (n)), full_name (move (fn))
  {
  }

  bool github_repository::
  empty () const
  {
    return name.empty ();
  }

  github_asset::
  github_asset (string n, string u, uint64_t s)
    : name (move (n)), browser_download_url (move (u)), size (s)
  {
  }

  bool github_asset::
  empty () const
  {
    return name.empty ();
  }

  github_release::
  github_release (string t, string n)
    : tag_name (move (t)), name (move (n))
  {
  }

  bool github_release::
  empty () const
  {
    return tag_name.empty ();
  }

  github_commit::
  github_commit (string s, string m)
    : sha (move (s)), message (move (m))
  {
  }

  bool github_commit::
  empty () const
  {
    return sha.empty ();
  }

  github_issue::
  github_issue (uint64_t n, string t)
    : number (n), title (move (t))
  {
  }

  bool github_issue::
  empty () const
  {
    return title.empty ();
  }

  github_branch::
  github_branch (string n)
    : name (move (n))
  {
  }

  bool github_branch::
  empty () const
  {
    return name.empty ();
  }

  github_tag::
  github_tag (string n)
    : name (move (n))
  {
  }

  bool github_tag::
  empty () const
  {
    return name.empty ();
  }

  optional<github_release::asset_type> github_release::
  find_asset (const string& n) const
  {
    auto i (find_if (assets.begin (),
                     assets.end (),
                     [&n] (const asset_type& a)
    {
      return a.name == n;
    }));

    return i != assets.end () ? optional<asset_type> (*i) : nullopt;
  }

  optional<github_release::asset_type> github_release::
  find_asset_regex (const string& p) const
  {
    regex re (p);

    auto i (find_if (assets.begin (),
                     assets.end (),
                     [&re] (const asset_type& a)
    {
      return regex_match (a.name, re);
    }));

    return i != assets.end () ? optional<asset_type> (*i) : nullopt;
  }

  // Comparison operators.
  //

  bool
  operator== (const github_user& x, const github_user& y) noexcept
  {
    return x.login == y.login && x.id == y.id;
  }

  bool
  operator!= (const github_user& x, const github_user& y) noexcept
  {
    return !(x == y);
  }

  bool
  operator== (const github_asset& x, const github_asset& y) noexcept
  {
    return x.id == y.id && x.name == y.name;
  }

  bool
  operator!= (const github_asset& x, const github_asset& y) noexcept
  {
    return !(x == y);
  }

  bool
  operator== (const github_release& x, const github_release& y) noexcept
  {
    return x.id == y.id && x.tag_name == y.tag_name;
  }

  bool
  operator!= (const github_release& x, const github_release& y) noexcept
  {
    return !(x == y);
  }
}
