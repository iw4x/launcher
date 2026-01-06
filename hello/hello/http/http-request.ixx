namespace hello
{
  template <typename S, typename B>
  inline typename basic_http_request<S, B>::string_type
  basic_http_request<S, B>::
  target () const
  {
    std::size_t pos (0);

    // Skip scheme if present (http:// or https://).
    //
    std::size_t scheme_end (url.find ("://"));
    if (scheme_end != string_type::npos)
      pos = scheme_end + 3;

    // Skip authority (host:port).
    //
    std::size_t path_start (url.find ('/', pos));
    if (path_start == string_type::npos)
      return string_type ("/");  // No path, return root

    return url.substr (path_start);
  }

  template <typename S, typename B>
  inline void basic_http_request<S, B>::
  normalize ()
  {
    if (body && !has_header (string_type ("Content-Length")) &&
        !has_header (string_type ("Transfer-Encoding")))
    {
      if constexpr (std::is_same<body_type, string_type>::value)
        set_header (string_type ("Content-Length"),
                   std::to_string (body->size ()));
    }

    if (!has_header (string_type ("Host")))
    {
      std::size_t pos (0);

      // Skip scheme if present.
      //
      std::size_t scheme_end (url.find ("://"));
      if (scheme_end != string_type::npos)
        pos = scheme_end + 3;

      // Extract host (everything from pos to next / or :).
      //
      std::size_t host_end (url.find_first_of ("/:", pos));
      if (host_end == string_type::npos)
        host_end = url.size ();

      string_type host (url.substr (pos, host_end - pos));
      if (!host.empty ())
        set_header (string_type ("Host"), host);
    }

    // Add User-Agent if not present.
    //
    if (!has_header (string_type ("User-Agent")))
      set_header (string_type ("User-Agent"),
                  string_type ("IW4x-Launcher-NP/1.0 (github.com/iw4x-x64/NP)"));
  }
}
