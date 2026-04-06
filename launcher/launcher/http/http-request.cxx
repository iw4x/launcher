#include <launcher/http/http-request.hxx>

using namespace std;

namespace launcher
{
  http_request::http_request (http_method m, string u, http_version v)
    : method (m),
      url (move (u)),
      version (v)
  {
  }

  http_request::http_request (http_method m,
                              string u,
                              http_headers h,
                              http_version v)
    : method (m),
      url (move (u)),
      version (v),
      headers (move (h))
  {
  }

  http_request::http_request (http_method m,
                              string u,
                              http_headers h,
                              string b,
                              http_version v)
    : method (m),
      url (move (u)),
      version (v),
      headers (move (h)),
      body (move (b))
  {
  }

  string
  http_request::target () const
  {
    // Extract the target part (path and query) from the URL. Note that if
    // the URL is absolute, we first need to skip the scheme in order to
    // properly bypass the authority (host and port) below.
    //
    size_t e (url.find ("://"));
    size_t p (e != string::npos ? e + 3 : 0);

    // Find where the path actually starts. If there is no path, we just
    // assume root.
    //
    size_t s (url.find ('/', p));

    return s != string::npos ? url.substr (s) : "/";
  }

  void
  http_request::normalize ()
  {
    // Supply any missing headers that are required for a well-formed request.
    // First, handle the body length. We need to set it if a body is present
    // but the size headers are missing.
    //
    if (body && !has_header ("Content-Length") &&
        !has_header ("Transfer-Encoding"))
      set_header ("Content-Length", std::to_string (body->size ()));

    // Next, deduce the Host header from the URL if it wasn't explicitly
    // specified.
    //
    if (!has_header ("Host"))
    {
      size_t e (url.find ("://"));
      size_t p (e != string::npos ? e + 3 : 0);

      // Find the end of the host part, which is delimited by either a port
      // separator or the beginning of the path.
      //
      size_t n (url.find_first_of ("/:", p));
      size_t l (n == string::npos ? url.size () - p : n - p);

      string h (url.substr (p, l));

      if (!h.empty ())
        set_header ("Host", h);
    }

    if (!has_header ("User-Agent"))
      set_header ("User-Agent", "IW4x-Launcher/1.1");
  }

  ostream&
  operator << (ostream& o, const http_request& r)
  {
    o << to_string (r.method) << ' ' << r.url << ' ' << r.version;
    return o;
  }
}
