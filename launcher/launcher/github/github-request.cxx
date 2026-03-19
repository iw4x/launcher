#include <launcher/github/github-request.hxx>

#include <sstream>

using namespace std;

namespace launcher
{
  github_request::
  github_request ()
    : method (method_type::get)
  {
  }

  github_request::
  github_request (method_type m, string e)
    : method (m), endpoint (move (e))
  {
  }

  github_request& github_request::
  with_token (string t)
  {
    // Stash the token so we can inject it into the authorization header later.
    //
    token = move (t);
    return *this;
  }

  github_request& github_request::
  with_body (string b)
  {
    body = move (b);
    return *this;
  }

  github_request& github_request::
  with_header (string k, string v)
  {
    headers[move (k)] = move (v);
    return *this;
  }

  github_request& github_request::
  with_query (string k, string v)
  {
    // Just keep it in the map for now. We will construct the actual query
    // string later when url() is called.
    //
    query_params[move (k)] = move (v);
    return *this;
  }

  github_request& github_request::
  with_per_page (uint32_t n)
  {
    // GitHub uses 'per_page' to dictate pagination limits.
    //
    return with_query ("per_page", to_string (n));
  }

  github_request& github_request::
  with_page (uint32_t n)
  {
    return with_query ("page", to_string (n));
  }

  github_request& github_request::
  with_state (const string& s)
  {
    return with_query ("state", s);
  }

  github_request& github_request::
  with_sort (const string& s)
  {
    return with_query ("sort", s);
  }

  github_request& github_request::
  with_direction (const string& d)
  {
    return with_query ("direction", d);
  }

  string github_request::
  url () const
  {
    ostringstream os;
    os << endpoint;

    // Only append the query string if we actually have parameters to send.
    //
    if (!query_params.empty ())
    {
      // keep track of the first parameter to handle the separator
      //
      bool f (true);

      for (const auto& [k, v] : query_params)
      {
        os << (f ? '?' : '&');
        os << k << '=' << v;
        f = false;
      }
    }

    return os.str ();
  }

  string github_request::
  method_string () const
  {
    // Map our internal enum to the uppercase string representations that
    // the underlying HTTP client expects.
    //
    switch (method)
    {
      case method_type::get:     return "GET";
      case method_type::post:    return "POST";
      case method_type::put:     return "PUT";
      case method_type::patch:   return "PATCH";
      case method_type::delete_: return "DELETE";
    }

    // Fall back to GET just in case we end up with something unexpected.
    //
    return "GET";
  }
}
