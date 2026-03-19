#include <launcher/download/download-request.hxx>

using namespace std;

namespace launcher
{
  download_request::
  download_request (string u, fs::path t)
    : urls ({move (u)}),
      target (move (t))
  {
  }

  download_request::
  download_request (vector<string> us, fs::path t)
    : urls (move (us)),
      target (move (t))
  {
  }

  bool download_request::
  valid () const
  {
    // A request is only meaningful if we have somewhere to download from and
    // somewhere to put it. We might later allow empty URLs if we are just
    // verifying an existing target, but for now be strict.
    //
    bool r (!urls.empty () && !target.empty ());
    return r;
  }

  bool
  operator== (const download_request& x, const download_request& y)
  {
    // Compare the lists and targets directly. Note that we rely on the
    // vector equality here, which means the exact order of fallback URLs
    // matters. This is usually what we want.
    //
    bool r (x.urls == y.urls && x.target == y.target);
    return r;
  }

  bool
  operator!= (const download_request& x, const download_request& y)
  {
    // No need to duplicate the logic, just delegate to operator==.
    //
    bool r (!(x == y));
    return r;
  }
}
