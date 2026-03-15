#include <launcher/download/download-request.hxx>

namespace launcher
{
  download_request::download_request (std::string url, fs::path tgt)
    : urls ({std::move (url)}),
      target (std::move (tgt))
  {
  }

  download_request::download_request (std::vector<std::string> us, fs::path tgt)
    : urls (std::move (us)),
      target (std::move (tgt))
  {
  }

  bool
  download_request::valid () const
  {
    return !urls.empty () && !target.empty ();
  }

  bool
  operator== (const download_request& x, const download_request& y)
  {
    return x.urls == y.urls &&
           x.target == y.target;
  }

  bool
  operator!= (const download_request& x, const download_request& y)
  {
    return !(x == y);
  }
}
