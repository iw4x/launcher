#include <algorithm>
#include <cctype>

namespace launcher
{
  // basic_http_headers
  //

  // Set a header, replacing any existing values.
  //
  // HTTP allows multiple headers with the same name (e.g., Set-Cookie), but we
  // enforces a "single value" semantics by clearing duplicates first.
  //
  template <typename S>
  inline void basic_http_headers<S>::
  set (string_type n, string_type v)
  {
    remove (n);
    fields.push_back (field_type (std::move (n), std::move (v)));
  }

  // Append a header.
  //
  template <typename S>
  inline void basic_http_headers<S>::
  add (string_type n, string_type v)
  {
    fields.push_back (field_type (std::move (n), std::move (v)));
  }

  // Retrieve the value of a header.
  //
  // Returns the first occurrence if multiple exist. The search is case-
  // insensitive as per RFC 7230.
  //
  template <typename S>
  inline std::optional<typename basic_http_headers<S>::string_type>
  basic_http_headers<S>::
  get (const string_type& n) const
  {
    auto match = [&n] (const field_type& f)
    {
      if (f.name.size () != n.size ())
        return false;

      for (std::size_t i (0); i < n.size (); ++i)
      {
        if (std::tolower (static_cast<unsigned char> (f.name[i])) !=
            std::tolower (static_cast<unsigned char> (n[i])))
          return false;
      }
      return true;
    };

    auto i (std::find_if (fields.begin (), fields.end (), match));
    return i != fields.end () ? std::optional<string_type> (i->value)
                              : std::nullopt;
  }

  template <typename S>
  inline bool basic_http_headers<S>::
  contains (const string_type& n) const
  {
    return get (n).has_value ();
  }

  // Remove all headers with the specified name.
  //
  template <typename S>
  inline void basic_http_headers<S>::
  remove (const string_type& n)
  {
    auto match = [&n] (const field_type& f)
    {
      if (f.name.size () != n.size ())
        return false;

      for (std::size_t i (0); i < n.size (); ++i)
      {
        if (std::tolower (static_cast<unsigned char> (f.name[i])) !=
            std::tolower (static_cast<unsigned char> (n[i])))
          return false;
      }
      return true;
    };

    fields.erase (std::remove_if (fields.begin (), fields.end (), match),
                  fields.end ());
  }
}
