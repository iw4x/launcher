#include <launcher/http/http-endpoint.hxx>
#include <stdexcept>

using namespace std;

namespace launcher
{
  http_endpoint::
  http_endpoint ()
    : default_method (http_method::get)
  {
  }

  http_endpoint::
  http_endpoint (string b, string p, http_method m)
    : base_url (move (b)),
      path_pattern (move (p)),
      default_method (m)
  {
  }

  string http_endpoint::
  build_url (const map<string, string>& ps) const
  {
    string p (path_pattern);

    // Substitute parameters into the path pattern. Note that the same
    // parameter might appear multiple times, so we keep replacing until
    // there are no more matches.
    //
    for (const auto& [k, v] : ps)
    {
      string t ("{" + k + "}");
      string::size_type i (0);

      while ((i = p.find (t, i)) != string::npos)
      {
        p.replace (i, t.size (), v);
        i += v.size ();
      }
    }

    // Verify we didn't miss anything. If there are still placeholders
    // left, it means the caller failed to provide all the necessary pieces.
    // Bail out.
    //
    if (p.find ('{') != string::npos)
      throw invalid_argument ("missing parameters for endpoint path");

    string r (base_url);

    // Combine the base URL and the path. We need to be careful with the
    // boundary: if both have a slash, we end up with a double slash, and
    // if neither has one, they run together.
    //
    if (!p.empty ())
    {
      if (!r.empty () && r.back () != '/')
        r += '/';

      if (p.front () == '/')
        p.erase (0, 1);

      r += p;
    }

    return r;
  }

  string http_endpoint::
  build_url () const
  {
    // Even without parameters, we should verify the pattern didn't expect
    // any. Otherwise it's likely a buggy call and we should complain.
    //
    if (path_pattern.find ('{') != string::npos)
      throw invalid_argument ("endpoint requires parameters but none provided");

    string r (base_url);

    // Same boundary dance as above.
    //
    if (!path_pattern.empty ())
    {
      if (!r.empty () && r.back () != '/')
        r += '/';

      string p (path_pattern);

      if (p.front () == '/')
        p.erase (0, 1);

      r += p;
    }

    return r;
  }

  bool
  operator== (const http_endpoint& x, const http_endpoint& y) noexcept
  {
    return x.base_url == y.base_url &&
           x.path_pattern == y.path_pattern &&
           x.default_method == y.default_method;
  }

  bool
  operator!= (const http_endpoint& x, const http_endpoint& y) noexcept
  {
    return !(x == y);
  }
}
