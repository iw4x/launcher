#include <algorithm>
#include <cctype>

namespace hello
{
  template <typename S>
  inline void basic_http_headers<S>::
  set (string_type name, string_type value)
  {
    // Remove existing fields with this name.
    //
    remove (name);

    // Add the new field.
    //
    fields.push_back (field_type (std::move (name), std::move (value)));
  }

  template <typename S>
  inline void basic_http_headers<S>::
  add (string_type name, string_type value)
  {
    fields.push_back (field_type (std::move (name), std::move (value)));
  }

  template <typename S>
  inline std::optional<typename basic_http_headers<S>::string_type>
  basic_http_headers<S>::
  get (const string_type& name) const
  {
    // Case-insensitive comparison.
    //
    auto compare_ci = [&name] (const field_type& f)
    {
      if (f.name.size () != name.size ())
        return false;

      for (std::size_t i (0); i < name.size (); ++i)
      {
        if (std::tolower (static_cast<unsigned char> (f.name[i])) !=
            std::tolower (static_cast<unsigned char> (name[i])))
          return false;
      }
      return true;
    };

    auto it (std::find_if (fields.begin (), fields.end (), compare_ci));
    return it != fields.end () ? std::optional<string_type> (it->value)
                                : std::nullopt;
  }

  template <typename S>
  inline bool basic_http_headers<S>::
  contains (const string_type& name) const
  {
    return get (name).has_value ();
  }

  template <typename S>
  inline void basic_http_headers<S>::
  remove (const string_type& name)
  {
    // Case-insensitive comparison.
    //
    auto compare_ci = [&name] (const field_type& f)
    {
      if (f.name.size () != name.size ())
        return false;

      for (std::size_t i (0); i < name.size (); ++i)
      {
        if (std::tolower (static_cast<unsigned char> (f.name[i])) !=
            std::tolower (static_cast<unsigned char> (name[i])))
          return false;
      }
      return true;
    };

    fields.erase (
      std::remove_if (fields.begin (), fields.end (), compare_ci),
      fields.end ());
  }
}
