#include <regex>
#include <stdexcept>

namespace launcher
{
  // Build the complete URL by substituting named parameters into the path
  // pattern.
  //
  // Note that we perform a simple string replacement for `{param}` tokens
  // rather than a full regex substitution. This avoids the compilation
  // overhead of std::regex for what is essentially a fixed-string replace
  // loop.
  //
  template <typename S>
  template <typename M>
  inline typename basic_http_endpoint<S>::string_type
  basic_http_endpoint<S>::
  build_url (const M& params) const
  {
    string_type p (path_pattern);

    // Iterate over the provided parameters and replace all occurrences of
    // their placeholders in the path.
    //
    for (const auto& [key, value] : params)
    {
      string_type t (string_type ("{") + key + string_type ("}"));

      typename string_type::size_type pos (0);
      while ((pos = p.find (t, pos)) != string_type::npos)
      {
        p.replace (pos, t.size (), value);
        pos += value.size ();
      }
    }

    // Verify that we haven't left any unexpanded placeholders. This catches
    // cases where the we forgot to supply a required parameter.
    //
    if (p.find (string_type ("{")) != string_type::npos)
      throw std::invalid_argument ("missing parameters for endpoint path");

    // Construct the result by joining the base URL and the expanded path.
    //
    // Note that we need to be careful with slashes here: we want exactly one
    // slash between the base and the path.
    //
    string_type r (base_url);

    if (!p.empty ())
    {
      if (!r.empty () && r.back () != '/')
        r += string_type ("/");

      if (!p.empty () && p.front () == '/')
        p = p.substr (1);

      r += p;
    }

    return r;
  }

  // Build the complete URL for endpoints that do not require any parameters.
  //
  template <typename S>
  inline typename basic_http_endpoint<S>::string_type
  basic_http_endpoint<S>::
  build_url () const
  {
    // First, sanity check that the pattern doesn't actually require parameters.
    // If it does, we're using the wrong overload (or the endpoint definition is
    // wrong).
    //
    if (path_pattern.find (string_type ("{")) != string_type::npos)
      throw std::invalid_argument (
        "endpoint requires parameters but none provided");

    string_type r (base_url);

    // Construct the result, see build_url (const M& params) const for details.
    //
    if (!path_pattern.empty ())
    {
      if (!r.empty () && r.back () != '/')
        r += string_type ("/");

      string_type p (path_pattern);
      if (!p.empty () && p.front () == '/')
        p = p.substr (1);

      r += p;
    }

    return r;
  }
}
