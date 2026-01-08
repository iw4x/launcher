#pragma once

#include <launcher/steam/steam-types.hxx>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <string>
#include <map>
#include <memory>
#include <istream>
#include <variant>

namespace launcher
{
  namespace asio = boost::asio;

  // Forward declaration for VDF node.
  //
  struct vdf_node;

  // VDF value - can be either a string or a nested object.
  //
  using vdf_value = std::variant<std::string, std::map<std::string, vdf_node>>;

  // VDF node in the parse tree.
  //
  struct vdf_node
  {
    vdf_value value;

    vdf_node () = default;

    explicit vdf_node (std::string s) : value (std::move (s)) {}

    explicit vdf_node (std::map<std::string, vdf_node> m)
      : value (std::move (m)) {}

    // Check if this node contains a string value.
    //
    bool
    is_string () const
    {
      return std::holds_alternative<std::string> (value);
    }

    // Check if this node contains an object value.
    //
    bool
    is_object () const
    {
      return std::holds_alternative<std::map<std::string, vdf_node>> (value);
    }

    // Get string value (throws if not a string).
    //
    const std::string&
    as_string () const
    {
      return std::get<std::string> (value);
    }

    // Get object value (throws if not an object).
    //
    const std::map<std::string, vdf_node>&
    as_object () const
    {
      return std::get<std::map<std::string, vdf_node>> (value);
    }

    // Get nested value by key path.
    //
    const vdf_node*
    find (const std::string& key) const;

    // Get string value at key path, or default if not found.
    //
    std::string
    get_string (const std::string& key, const std::string& default_value = "") const;

    // Get nested object at key path, or nullptr if not found.
    //
    const std::map<std::string, vdf_node>*
    get_object (const std::string& key) const;
  };

  // VDF parser for Valve Data Format files.
  //
  class vdf_parser
  {
  public:
    // Parse VDF from string synchronously.
    //
    static vdf_node
    parse (const std::string& vdf_str);

    // Parse VDF from file synchronously.
    //
    static vdf_node
    parse_file (const fs::path& file);

    // Parse VDF from stream synchronously.
    //
    static vdf_node
    parse_stream (std::istream& is);

    // Parse VDF from string asynchronously.
    //
    static asio::awaitable<vdf_node>
    parse_async (asio::io_context& ioc, const std::string& vdf_str);

    // Parse VDF from file asynchronously.
    //
    static asio::awaitable<vdf_node>
    parse_file_async (asio::io_context& ioc, const fs::path& file);

  private:
    // Internal parser state.
    //
    struct parser_state
    {
      const char* current;
      const char* end;
      size_t line;
      size_t column;

      parser_state (const std::string& str)
          : current (str.data ()),
            end (str.data () + str.size ()),
            line (1),
            column (1)
      {}
    };

    // Skip whitespace and comments.
    //
    static void
    skip_whitespace (parser_state& state);

    // Parse a quoted string.
    //
    static std::string
    parse_string (parser_state& state);

    // Parse a key-value pair.
    //
    static std::pair<std::string, vdf_node>
    parse_pair (parser_state& state);

    // Parse an object (map of key-value pairs).
    //
    static std::map<std::string, vdf_node>
    parse_object (parser_state& state);

    // Peek next non-whitespace character.
    //
    static char
    peek_char (parser_state& state);

    // Consume next character.
    //
    static char
    next_char (parser_state& state);
  };

  // Steam-specific VDF parsers.
  //

  // Parse libraryfolders.vdf file.
  //
  asio::awaitable<std::vector<steam_library>>
  parse_library_folders (asio::io_context& ioc, const fs::path& vdf_file);

  // Parse appmanifest_*.acf file.
  //
  asio::awaitable<steam_app_manifest>
  parse_app_manifest (asio::io_context& ioc, const fs::path& acf_file);

  // Parse config.vdf file.
  //
  asio::awaitable<vdf_node>
  parse_config_vdf (asio::io_context& ioc, const fs::path& vdf_file);
}
