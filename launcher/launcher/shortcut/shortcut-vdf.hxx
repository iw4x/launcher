#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace launcher
{
  class vdf_object
  {
  public:
    enum class kind : std::uint8_t
    {
      object = 0x00,
      string = 0x01,
      number = 0x02
    };

    static vdf_object
    parse (const std::vector<char>&);

    std::vector<char>
    serialize () const;

    bool
    empty () const noexcept;

    std::size_t
    size () const noexcept;

    std::vector<std::string>
    keys () const;

    const std::string*
    string (std::string_view) const;

    const std::int32_t*
    number (std::string_view) const;

    vdf_object*
    object (std::string_view);

    const vdf_object*
    object (std::string_view) const;

    void
    set (std::string key, std::string value);

    void
    set (std::string key, std::int32_t value);

    vdf_object&
    set_object (std::string key);

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

      std::vector<vdf_object> o;
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
