// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <print>
#include <string>
#include <vector>
#include <cstdlib>      // exit()
#include <iostream>
#include <iterator>
#include <optional>
#include <exception>
#include <string_view>

#include <boost/json.hpp>

#include <launcher/github/github-api.hxx>

using namespace std;
using namespace launcher;

namespace
{
  [[noreturn]] void
  fail (const string& m)
  {
    println (cerr, "error: {}", m);
    exit (1);
  }

  // Parse the JSON value read from stdin.
  //
  json::value
  read_json ()
  {
    string s ((istreambuf_iterator<char> (cin)), istreambuf_iterator<char> ());
    return json::parse (s);
  }

  void
  print_release (const github_release& r)
  {
    println ("tag '{}'\n"
             "name '{}'\n"
             "draft {}\n"
             "prerelease {}\n"
             "author '{}'",
             r.tag_name,
             r.name,
             r.draft,
             r.prerelease,
             r.author.login);

    for (const github_asset& a: r.assets)
      println ("asset {} {} {}", a.name, a.size, a.browser_download_url);
  }
}

// Usage: argv[0] <command> [<argument>...]
//
// release
//    Read a release object in the GitHub REST API form from stdin and print
//    it in the following form, with an asset line for each asset:
//
//    tag '<tag>'
//    name '<name>'
//    draft <bool>
//    prerelease <bool>
//    author '<login>'
//    asset <name> <size> <url>
//
// releases
//    Read an array of release objects from stdin and print the tag of each,
//    one per line.
//
// find <name>
//    Read a release object from stdin and print the name and download URL
//    of its asset with the name. Fail if there is no such asset.
//
// match <regex>
//    As find but look for the first asset whose whole name matches the
//    regular expression.
//
// endpoint releases <owner> <repo>
//    Print the API URL of the repository's release list.
//
// endpoint latest <owner> <repo>
//    Print the API URL of the repository's latest release.
//
// endpoint tag <owner> <repo> <tag>
//    Print the API URL of the repository's release with the tag.
//
// url <endpoint> [<key>=<value>...]
//    Print the URL of a request to the endpoint with the query parameters.
//
int
main (int argc, char* argv[])
{
  vector<string> as (argv + 1, argv + argc);
  string_view c (!as.empty () ? string_view (as[0]) : string_view ());

  try
  {
    if (c == "release" && as.size () == 1)
    {
      print_release (github_api_traits::parse_release (read_json ()));
    }
    else if (c == "releases" && as.size () == 1)
    {
      for (const github_release& r:
             github_api_traits::parse_releases (read_json ()))
        println ("{}", r.tag_name);
    }
    else if ((c == "find" || c == "match") && as.size () == 2)
    {
      github_release r (github_api_traits::parse_release (read_json ()));

      optional<github_asset> a (c == "find"
                                ? r.find_asset (as[1])
                                : r.find_asset_regex (as[1]));
      if (!a)
        fail ("no asset matching '" + as[1] + "'");

      println ("{} {}", a->name, a->browser_download_url);
    }
    else if (c == "endpoint" && as.size () >= 4)
    {
      const string& o (as[2]);
      const string& n (as[3]);

      if (as[1] == "releases" && as.size () == 4)
        println ("{}", github_endpoint::repo_releases (o, n));
      else if (as[1] == "latest" && as.size () == 4)
        println ("{}", github_endpoint::repo_release_latest (o, n));
      else if (as[1] == "tag" && as.size () == 5)
        println ("{}", github_endpoint::repo_release_tag (o, n, as[4]));
      else
        fail ("invalid endpoint arguments");
    }
    else if (c == "url" && as.size () >= 2)
    {
      github_request r (github_request::method_type::get, as[1]);

      for (size_t i (2); i != as.size (); ++i)
      {
        size_t p (as[i].find ('='));

        if (p == string::npos)
          fail ("invalid query parameter '" + as[i] + "'");

        r.with_query (as[i].substr (0, p), as[i].substr (p + 1));
      }

      println ("{}", r.url ());
    }
    else
      fail ("invalid arguments\n"
            "  info: usage: driver <command> [<argument>...]");
  }
  catch (const exception& e)
  {
    fail (e.what ());
  }
}
