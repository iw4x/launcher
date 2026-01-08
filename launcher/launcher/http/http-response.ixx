#include <charconv>

namespace launcher
{
  // Parse the Content-Length header.
  //
  // Returns the length in bytes if the header exists and contains a valid
  // non-negative integer. Otherwise returns nullopt.
  //
  template <typename S, typename B>
  inline std::optional<std::uint64_t> basic_http_response<S, B>::
  content_length () const
  {
    auto v (get_header (string_type ("Content-Length")));

    if (!v)
      return std::nullopt;

    std::uint64_t n (0);

    // Note that we use std::from_chars for locale-independent parsing.
    //
    auto r (std::from_chars (v->data (),
                             v->data () + v->size (),
                             n));

    if (r.ec == std::errc ())
      return n;

    return std::nullopt;
  }
}
