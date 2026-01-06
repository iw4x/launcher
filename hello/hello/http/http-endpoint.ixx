#include <regex>
#include <stdexcept>

namespace hello
{
  // basic_http_endpoint
  //
  template <typename S>
  template <typename M>
  inline typename basic_http_endpoint<S>::string_type
  basic_http_endpoint<S>::
  build_url (const M& params) const
  {
    string_type path (path_pattern);

    // Replace {param} placeholders with actual values.
    //
    for (const auto& [key, value] : params)
    {
      string_type placeholder (string_type ("{") + key + string_type ("}"));

      typename string_type::size_type pos (0);
      while ((pos = path.find (placeholder, pos)) != string_type::npos)
      {
        path.replace (pos, placeholder.size (), value);
        pos += value.size ();
      }
    }

    // Check if any placeholders remain (missing parameters).
    //
    if (path.find (string_type ("{")) != string_type::npos)
    {
      throw std::invalid_argument (
        "missing parameters for endpoint path pattern");
    }

    // Build the complete URL.
    //
    string_type result (base_url);

    if (!path.empty ())
    {
      // Ensure proper path separator.
      //
      if (!result.empty () && result.back () != '/')
        result += string_type ("/");

      if (!path.empty () && path.front () == '/')
        path = path.substr (1);

      result += path;
    }

    return result;
  }

  template <typename S>
  inline typename basic_http_endpoint<S>::string_type
  basic_http_endpoint<S>::
  build_url () const
  {
    // For endpoints without parameters, just append the pattern.
    //
    if (path_pattern.find (string_type ("{")) != string_type::npos)
    {
      throw std::invalid_argument (
        "endpoint requires parameters but none provided");
    }

    string_type result (base_url);

    if (!path_pattern.empty ())
    {
      if (!result.empty () && result.back () != '/')
        result += string_type ("/");

      string_type pattern (path_pattern);
      if (!pattern.empty () && pattern.front () == '/')
        pattern = pattern.substr (1);

      result += pattern;
    }

    return result;
  }
}
