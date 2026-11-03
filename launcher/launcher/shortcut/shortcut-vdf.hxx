// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace launcher
{
  // Object in Valve's binary key-values format, which Steam uses for
  // shortcuts.vdf.
  //
  // An object is a sequence of entries terminated by the 0x08 byte. Each
  // entry starts with its kind byte and its NUL-terminated key, followed by
  // the value. Strings are NUL-terminated and numbers are 32-bit
  // little-endian. We keep the entries in their order, so serializing a
  // parsed document gives back the same bytes.
  //
  class vdf_object
  {
  public:
    enum class kind : std::uint8_t
    {
      object = 0x00,
      string = 0x01,
      number = 0x02
    };

    // Parse the document. Throw std::runtime_error if it is malformed,
    // including when there is data after the top-level object.
    //
    static vdf_object
    parse (const std::vector<char>&);

    // Return the object as a document. Throw std::runtime_error if a key or
    // string contains NUL.
    //
    std::vector<char>
    serialize () const;

    bool
    empty () const noexcept;

    std::size_t
    size () const noexcept;

    // Return the keys in the entry order.
    //
    std::vector<std::string>
    keys () const;

    // Return the value of the first entry with the key if it is of this
    // kind and NULL otherwise.
    //
    const std::string*
    string (std::string_view) const;

    const std::int32_t*
    number (std::string_view) const;

    vdf_object*
    object (std::string_view);

    const vdf_object*
    object (std::string_view) const;

    // Replace the kind and value of the first entry with the key, appending
    // the entry if there is none.
    //
    void
    set (std::string key, std::string value);

    void
    set (std::string key, std::int32_t value);

    // As above but for an object, which starts empty. Return the object.
    //
    vdf_object&
    set_object (std::string key);

    // Remove the first entry with the key, if any.
    //
    void
    erase (std::string_view);

  private:
    class reader;

    void
    parse_map (reader&);

    struct entry
    {
      std::string key;
      kind k = kind::string;

      std::string s;
      std::int32_t n = 0;

      std::vector<vdf_object> o; // One element for an object (the type is
                                 // still incomplete here).
    };

    const entry*
    find (std::string_view, kind) const;

    entry&
    append (std::string key, kind);

    entry&
    put (std::string key, kind);

    std::vector<entry> es_;
  };
}
