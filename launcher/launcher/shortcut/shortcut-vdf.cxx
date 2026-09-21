#include <launcher/shortcut/shortcut-vdf.hxx>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

using namespace std;

namespace launcher
{
  namespace
  {
    constexpr char vdf_end (0x08);

    void
    put_text (vector<char>& d, const string& s)
    {
      if (s.find ('\0') != string::npos)
        throw runtime_error ("embedded NUL in key-values string");

      d.insert (d.end (), s.begin (), s.end ());
      d.push_back ('\0');
    }

    void
    put_number (vector<char>& d, int32_t n)
    {
      uint32_t v (static_cast<uint32_t> (n));

      for (unsigned s (0); s != 32; s += 8)
        d.push_back (static_cast<char> ((v >> s) & 0xff));
    }
  }

  class vdf_object::reader
  {
  public:
    reader (const vector<char>& d) : d_ (d) {}

    bool
    done () const noexcept
    {
      return i_ == d_.size ();
    }

    char
    byte ()
    {
      if (i_ == d_.size ())
        throw runtime_error ("truncated key-values document");

      return d_[i_++];
    }

    std::string
    text ()
    {
      auto b (d_.begin () + static_cast<ptrdiff_t> (i_));
      auto e (std::find (b, d_.end (), '\0'));

      if (e == d_.end ())
        throw runtime_error ("unterminated string in key-values document");

      std::string r (b, e);
      i_ += r.size () + 1;

      return r;
    }

    int32_t
    number ()
    {
      if (d_.size () - i_ < 4)
        throw runtime_error ("truncated number in key-values document");

      uint32_t v (0);

      for (unsigned s (0); s != 32; s += 8)
        v |= static_cast<uint32_t> (
               static_cast<unsigned char> (d_[i_++])) << s;

      return static_cast<int32_t> (v);
    }

  private:
    const vector<char>& d_;
    size_t i_ = 0;
  };

  void vdf_object::
  parse_map (reader& r)
  {
    for (char t (r.byte ()); t != vdf_end; t = r.byte ())
    {
      std::string k (r.text ());

      switch (t)
      {
      case static_cast<char> (kind::object):
        {
          entry& e (append (std::move (k), kind::object));
          e.o.emplace_back ();
          e.o.front ().parse_map (r);
          break;
        }

      case static_cast<char> (kind::string):
        append (std::move (k), kind::string).s = r.text ();
        break;

      case static_cast<char> (kind::number):
        append (std::move (k), kind::number).n = r.number ();
        break;

      default:
        throw runtime_error ("unsupported key-values entry type " +
                             std::to_string (static_cast<int> (t)));
      }
    }
  }

  vdf_object vdf_object::
  parse (const vector<char>& d)
  {
    reader r (d);
    vdf_object o;

    o.parse_map (r);

    if (!r.done ())
      throw runtime_error ("trailing data after key-values document");

    return o;
  }

  vector<char> vdf_object::
  serialize () const
  {
    vector<char> d;

    for (const entry& e : es_)
    {
      d.push_back (static_cast<char> (e.k));
      put_text (d, e.key);

      switch (e.k)
      {
      case kind::object:
        {
          vector<char> n (e.o.front ().serialize ());
          d.insert (d.end (), n.begin (), n.end ());
          break;
        }

      case kind::string:
        put_text (d, e.s);
        break;

      case kind::number:
        put_number (d, e.n);
        break;
      }
    }

    d.push_back (vdf_end);
    return d;
  }

  bool vdf_object::
  empty () const noexcept
  {
    return es_.empty ();
  }

  size_t vdf_object::
  size () const noexcept
  {
    return es_.size ();
  }

  vector<std::string> vdf_object::
  keys () const
  {
    vector<std::string> r;
    r.reserve (es_.size ());

    for (const entry& e : es_)
      r.push_back (e.key);

    return r;
  }

  const vdf_object::entry* vdf_object::
  find (string_view k, kind t) const
  {
    for (const entry& e : es_)
    {
      if (e.key == k)
        return e.k == t ? &e : nullptr;
    }

    return nullptr;
  }

  const std::string* vdf_object::
  string (string_view k) const
  {
    const entry* e (find (k, kind::string));
    return e != nullptr ? &e->s : nullptr;
  }

  const int32_t* vdf_object::
  number (string_view k) const
  {
    const entry* e (find (k, kind::number));
    return e != nullptr ? &e->n : nullptr;
  }

  const vdf_object* vdf_object::
  object (string_view k) const
  {
    const entry* e (find (k, kind::object));
    return e != nullptr ? &e->o.front () : nullptr;
  }

  vdf_object* vdf_object::
  object (string_view k)
  {
    return const_cast<vdf_object*> (
      static_cast<const vdf_object&> (*this).object (k));
  }

  vdf_object::entry& vdf_object::
  append (std::string k, kind t)
  {
    es_.push_back (entry {std::move (k), t, "", 0, {}});
    return es_.back ();
  }

  vdf_object::entry& vdf_object::
  put (std::string k, kind t)
  {
    auto i (std::find_if (es_.begin (), es_.end (), [&k] (const entry& e)
    {
      return e.key == k;
    }));

    if (i == es_.end ())
      i = es_.insert (es_.end (), entry {std::move (k), t, "", 0, {}});
    else
    {
      i->k = t;
      i->s.clear ();
      i->n = 0;
      i->o.clear ();
    }

    return *i;
  }

  void vdf_object::
  set (std::string k, std::string v)
  {
    put (std::move (k), kind::string).s = std::move (v);
  }

  void vdf_object::
  set (std::string k, int32_t v)
  {
    put (std::move (k), kind::number).n = v;
  }

  vdf_object& vdf_object::
  set_object (std::string k)
  {
    entry& e (put (std::move (k), kind::object));

    if (e.o.empty ())
      e.o.emplace_back ();

    return e.o.front ();
  }

  void vdf_object::
  erase (string_view k)
  {
    auto i (std::find_if (es_.begin (), es_.end (), [&k] (const entry& e)
    {
      return e.key == k;
    }));

    if (i != es_.end ())
      es_.erase (i);
  }
}
