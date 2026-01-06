#include <charconv>

namespace hello
{
  template <typename S, typename B>
  inline std::optional<std::uint64_t> basic_http_response<S, B>::
  content_length () const
  {
    auto cl (get_header (string_type ("Content-Length")));

    if (!cl)
      return std::nullopt;

    std::uint64_t len (0);

    // Parse the content length.
    //
    auto result (std::from_chars (cl->data (),
                                  cl->data () + cl->size (),
                                  len));

    if (result.ec == std::errc ())
      return len;

    return std::nullopt;
  }
}
