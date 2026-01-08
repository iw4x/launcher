namespace launcher
{
  // Extract the target path from the full URL.
  //
  // The HTTP request line requires just the path (and query), not the full
  // absolute URI. So we strip off the scheme and authority components.
  //
  template <typename S, typename B>
  inline typename basic_http_request<S, B>::string_type
  basic_http_request<S, B>::
  target () const
  {
    std::size_t pos (0);

    // Skip the scheme (e.g., http://) if it exists.
    //
    std::size_t scheme_end (url.find ("://"));
    if (scheme_end != string_type::npos)
      pos = scheme_end + 3;

    // Find the start of the path.
    //
    // If we can't find a slash after the scheme/authority, it means the URL
    // is something like "http://example.com", which implies the root target.
    //
    std::size_t path_start (url.find ('/', pos));
    if (path_start == string_type::npos)
      return string_type ("/");  // No path, return root

    return url.substr (path_start);
  }

  // Normalize the request headers.
  //
  template <typename S, typename B>
  inline void basic_http_request<S, B>::
  normalize ()
  {
    // Calculate Content-Length.
    //
    // If we have a body and no length/encoding headers, set it now.
    //
    if (body &&
        !has_header (string_type ("Content-Length")) &&
        !has_header (string_type ("Transfer-Encoding")))
    {
      if constexpr (std::is_same<body_type, string_type>::value)
        set_header (string_type ("Content-Length"),
                    std::to_string (body->size ()));
    }

    // Set Host header.
    //
    // Required by HTTP/1.1. We parse the authority section of the URL to
    // extract the hostname and optional port.
    //
    if (!has_header (string_type ("Host")))
    {
      std::size_t p (0);

      if (std::size_t n (url.find ("://")); n != string_type::npos)
        p = n + 3;

      // The host segment ends at the next slash (path) or colon (port),
      // though typically the Host header includes the port if non-default.
      //
      // Here we just grab everything until the path start.
      //
      std::size_t n (url.find_first_of ("/:", p));
      if (n == string_type::npos)
        n = url.size ();

      string_type h (url.substr (p, n - p));
      if (!h.empty ())
        set_header (string_type ("Host"), h);
    }

    // Set default User-Agent.
    //
    if (!has_header (string_type ("User-Agent")))
      set_header (string_type ("User-Agent"),
                  string_type ("IW4x-Launcher-NP/1.1"));
  }
}
