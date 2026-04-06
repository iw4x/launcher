#include <launcher/http/http-response.hxx>
#include <charconv>

namespace launcher
{
  http_response::http_response ()
    : status (http_status::ok) {}

  http_response::http_response (http_status s, http_version v)
    : status (s),
      version (v) {}

  http_response::http_response (http_status s, std::string r, http_version v)
    : status (s),
      version (v),
      reason (std::move (r)) {}

  http_response::http_response (http_status s, http_headers h, http_version v)
    : status (s),
      version (v),
      headers (std::move (h)) {}

  http_response::http_response (http_status s,
                                http_headers h,
                                std::string b,
                                http_version v)
    : status (s),
      version (v),
      headers (std::move (h)),
      body (std::move (b)) {}

  std::optional<std::uint64_t>
  http_response::content_length () const
  {
    auto v (get_header ("Content-Length"));
    if (!v)
      return std::nullopt;

    std::uint64_t n (0);
    auto r (std::from_chars (v->data (), v->data () + v->size (), n));

    if (r.ec == std::errc ())
      return n;

    return std::nullopt;
  }

  std::ostream&
  operator << (std::ostream& o, const http_response& r)
  {
    o << r.version << ' ' << static_cast<std::uint16_t> (r.status);
    if (!r.reason.empty ())
      o << ' ' << r.reason;
    return o;
  }
}
