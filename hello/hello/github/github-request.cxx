#include <hello/github/github-request.hxx>

#include <sstream>

using namespace std;

namespace hello
{
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
}
