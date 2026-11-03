// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <print>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>      // exit()
#include <charconv>     // from_chars()
#include <iostream>
#include <stdexcept>
#include <string_view>

#include <launcher/shortcut/shortcut-vdf.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace launcher;

namespace
{
  [[noreturn]] void
  fail (const string& m)
  {
    println (cerr, "error: {}", m);
    exit (1);
  }

  // Read the bytes from stdin (see main() for the notation).
  //
  vector<char>
  read_bytes ()
  {
    vector<char> r;

    auto hex = [] (char c) -> int
    {
      return c >= '0' && c <= '9' ? c - '0'      :
             c >= 'a' && c <= 'f' ? c - 'a' + 10 :
             c >= 'A' && c <= 'F' ? c - 'A' + 10 :
                                    -1;
    };

    string s ((istreambuf_iterator<char> (cin)), istreambuf_iterator<char> ());

    for (size_t i (0), n (s.size ()); i != n; )
    {
      char c (s[i]);

      if (c == ' ' || c == '\n')
      {
        ++i;
      }
      else if (c == '\'')
      {
        size_t e (s.find ('\'', i + 1));

        if (e == string::npos)
          fail ("unterminated text in bytes");

        r.insert (r.end (), s.begin () + i + 1, s.begin () + e);
        i = e + 1;
      }
      else
      {
        int h (i + 1 != n ? hex (s[i]) : -1);
        int l (i + 1 != n ? hex (s[i + 1]) : -1);

        if (h == -1 || l == -1)
          fail ("invalid byte at offset " + to_string (i));

        r.push_back (static_cast<char> (h * 16 + l));
        i += 2;
      }
    }

    return r;
  }

  // Print the bytes to stdout (see main() for the notation).
  //
  void
  print_bytes (const vector<char>& d)
  {
    string r;
    bool text (false);

    for (char c: d)
    {
      bool t (c >= 0x20 && c <= 0x7e && c != '\'');

      if (text && !t)
      {
        r += '\'';
        text = false;
      }

      if (!r.empty () && !text)
        r += ' ';

      if (t && !text)
      {
        r += '\'';
        text = true;
      }

      if (t)
        r += c;
      else
        r += format ("{:02x}", static_cast<unsigned char> (c));
    }

    if (text)
      r += '\'';

    println ("{}", r);
  }

  // Print the object's entries in the tree notation (see main()), indented
  // by the number of spaces.
  //
  // Note that we discover the kind of each entry through the typed
  // accessors, only one of which should find it.
  //
  void
  print_tree (const vdf_object& o, size_t indent)
  {
    vector<string> ks (o.keys ());
    assert (ks.size () == o.size () && ks.empty () == o.empty ());

    string in (indent, ' ');

    for (const string& k: ks)
    {
      const vdf_object* v (o.object (k));
      const string*     s (o.string (k));
      const int32_t*    n (o.number (k));

      assert ((v != nullptr) + (s != nullptr) + (n != nullptr) == 1);

      if (v != nullptr && v->empty ())
        println ("{}{} {{}}", in, k);
      else if (v != nullptr)
      {
        println ("{}{} {{", in, k);
        print_tree (*v, indent + 2);
        println ("{}}}", in);
      }
      else if (s != nullptr)
        println ("{}{} = '{}'", in, k, *s);
      else
        println ("{}{} : {}", in, k, *n);
    }
  }

  // Build the object from the tree read from stdin (see main() for the
  // notation), applying each entry as it comes.
  //
  vdf_object
  read_tree ()
  {
    vdf_object r;
    vector<vdf_object*> os {&r};

    for (string l; getline (cin, l); )
    {
      string_view v (l);
      v.remove_prefix (min (v.find_first_not_of (' '), v.size ()));

      if (v.empty ())
        continue;

      vdf_object& o (*os.back ());
      size_t p;

      if (v == "}")
      {
        if (os.size () == 1)
          fail ("unexpected '}'");

        os.pop_back ();
      }
      else if (v.starts_with ("- "))
      {
        o.erase (v.substr (2));
      }
      else if (v.ends_with (" {}"))
      {
        o.set_object (string (v.substr (0, v.size () - 3)));
      }
      else if (v.ends_with (" {"))
      {
        os.push_back (&o.set_object (string (v.substr (0, v.size () - 2))));
      }
      else if ((p = v.find (" = '")) != string_view::npos && v.ends_with ('\''))
      {
        // Turn each \0 into NUL.
        //
        string s;
        for (string_view t (v.substr (p + 4, v.size () - p - 5));
             !t.empty ();
             t.remove_prefix (1))
        {
          if (t.starts_with ("\\0"))
          {
            s += '\0';
            t.remove_prefix (1);
          }
          else
            s += t.front ();
        }

        o.set (string (v.substr (0, p)), move (s));
      }
      else if ((p = v.find (" : ")) != string_view::npos)
      {
        string_view t (v.substr (p + 3));
        int32_t n;

        auto [e, ec] (from_chars (t.data (), t.data () + t.size (), n));

        if (ec != errc () || e != t.data () + t.size ())
          fail ("invalid number in '" + l + "'");

        o.set (string (v.substr (0, p)), n);
      }
      else
        fail ("invalid entry '" + l + "'");
    }

    if (os.size () != 1)
      fail ("unterminated object");

    return r;
  }
}

// Usage: argv[0] (parse|serialize)
//
// parse
//    Parse the binary key-values document read from stdin and print its
//    tree. Fail if serializing the result doesn't reproduce the bytes.
//
// serialize
//    Build an object from the tree read from stdin, applying each entry
//    with the corresponding modifier, and print its serialized bytes.
//
// The bytes are space-separated two-digit hexadecimal numbers and quoted
// runs of text, each run standing for its characters. So a string still
// needs its 00 terminator. For example:
//
//    00 'shortcuts' 00 02 'appid' 00 11 22 33 84 08 08
//
// The tree has one entry per line, with the nested entries indented by two
// spaces, in the following forms (with \0 standing for NUL in a string):
//
//    <key> {
//    <key> {}
//    }
//    <key> = '<string>'
//    <key> : <number>
//    - <key>
//
// The last form erases the entry and only makes sense for serialize.
//
int
main (int argc, char* argv[])
{
  string_view c (argc == 2 ? argv[1] : "");

  try
  {
    if (c == "parse")
    {
      vector<char> d (read_bytes ());
      vdf_object o (vdf_object::parse (d));

      if (o.serialize () != d)
        fail ("serialized bytes differ from parsed");

      print_tree (o, 0);
    }
    else if (c == "serialize")
      print_bytes (read_tree ().serialize ());
    else
      fail ("invalid arguments\n"
            "  info: usage: driver (parse|serialize)");
  }
  catch (const runtime_error& e)
  {
    fail (e.what ());
  }
}
