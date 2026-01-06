#include <hello/github/github-types.hxx>

#include <regex>
#include <algorithm>

using namespace std;

namespace hello
{
  optional<github_release::asset_type> github_release::
  find_asset (const string& name) const
  {
    auto i (find_if (assets.begin (),
                     assets.end (),
                     [&name] (const asset_type& a)
    {
      return a.name == name;
    }));

    return i != assets.end () ? optional<asset_type> (*i) : nullopt;
  }

  optional<github_release::asset_type> github_release::
  find_asset_regex (const string& pattern) const
  {
    regex re (pattern);

    auto i (find_if (assets.begin (),
                          assets.end (),
                          [&re] (const asset_type& a)
    {
      return regex_match (a.name, re);
    }));

    return i != assets.end () ? optional<asset_type> (*i) : nullopt;
  }
}
