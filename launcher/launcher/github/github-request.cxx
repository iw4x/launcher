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
  github_request (method_type m, string ep)
    : method (m), endpoint (move (ep))
  {
  }

  github_request& github_request::
  with_token (string t)
  {
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
  with_header (string key, string value)
  {
    headers[move (key)] = move (value);
    return *this;
  }

  github_request& github_request::
  with_query (string key, string value)
  {
    query_params[move (key)] = move (value);
    return *this;
  }

  github_request& github_request::
  with_per_page (uint32_t n)
  {
    return with_query ("per_page", to_string (n));
  }

  github_request& github_request::
  with_page (uint32_t n)
  {
    return with_query ("page", to_string (n));
  }

  github_request& github_request::
  with_state (const string& state)
  {
    return with_query ("state", state);
  }

  github_request& github_request::
  with_sort (const string& sort)
  {
    return with_query ("sort", sort);
  }

  github_request& github_request::
  with_direction (const string& dir)
  {
    return with_query ("direction", dir);
  }

  string github_request::
  url () const
  {
    ostringstream os;
    os << endpoint;

    if (!query_params.empty ())
    {
      bool first (true);
      for (const auto& [key, value] : query_params)
      {
        os << (first ? '?' : '&');
        os << key << '=' << value;
        first = false;
      }
    }

    return os.str ();
  }

  string github_request::
  method_string () const
  {
    switch (method)
    {
      case method_type::get:     return "GET";
      case method_type::post:    return "POST";
      case method_type::put:     return "PUT";
      case method_type::patch:   return "PATCH";
      case method_type::delete_: return "DELETE";
    }
    return "GET";
  }
}
