#include <launcher/steam/steam-parser.hxx>

#include <fstream>
#include <sstream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <stdexcept>

#include <boost/asio/post.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <launcher/launcher-log.hxx>

using namespace std;

namespace launcher
{
  // Try to find a child node by key. Returns nullptr if this node is not an
  // object or the key doesn't exist.
  //
  const vdf_node* vdf_node::
  find (const string& key) const
  {
    if (!is_object ())
      return nullptr;

    const auto& obj (as_object ());
    auto it (obj.find (key));
    return it != obj.end () ? &it->second : nullptr;
  }

  // Helper to retrieve a string value safely.
  //
  string vdf_node::
  get_string (const string& key, const string& default_value) const
  {
    const vdf_node* n (find (key));

    if (n != nullptr && n->is_string ())
      return n->as_string ();

    return default_value;
  }

  // Helper to retrieve a nested object safely.
  //
  const map<string, vdf_node>* vdf_node::
  get_object (const string& key) const
  {
    const vdf_node* n (find (key));

    if (n != nullptr && n->is_object ())
      return &n->as_object ();

    return nullptr;
  }

  // vdf_parser
  //

  // Skip over any whitespace characters.
  //
  // We also handle C++-style comments (//) here since they effectively act as
  // whitespace between tokens in VDF.
  //
  void vdf_parser::
  skip_whitespace (parser_state& s)
  {
    while (s.current < s.end)
    {
      char c (*s.current);

      // standard whitespace.
      //
      if (std::isspace (static_cast<unsigned char> (c)))
      {
        if (c == '\n')
        {
          ++s.line;
          s.column = 1;
        }
        else
        {
          ++s.column;
        }
        ++s.current;
        continue;
      }

      // Comments (// style).
      //
      if (c == '/' && (s.current + 1) < s.end && *(s.current + 1) == '/')
      {
        // Skip until the end of the line.
        //
        while (s.current < s.end && *s.current != '\n')
          ++s.current;

        continue;
      }

      break;
    }
  }

  // Peek at the next significant character without consuming it.
  //
  char vdf_parser::
  peek_char (parser_state& s)
  {
    skip_whitespace (s);
    return (s.current < s.end) ? *s.current : '\0';
  }

  // Consume and return the next significant character.
  //
  char vdf_parser::
  next_char (parser_state& s)
  {
    skip_whitespace (s);

    if (s.current >= s.end)
      return '\0';

    char c (*s.current);
    ++s.current;
    ++s.column;
    return c;
  }

  // Parse a string token.
  //
  // VDF strings can be quoted or unquoted. If quoted, we need to handle escape
  // sequences. If unquoted, they are terminated by whitespace or structural
  // characters ('{', '}', '"').
  //
  string vdf_parser::
  parse_string (parser_state& s)
  {
    skip_whitespace (s);

    if (s.current >= s.end)
    {
      launcher::log::error (categories::steam{}, "unexpected end of input at line {}, column {}", s.line, s.column);
      throw runtime_error ("unexpected end of input");
    }

    bool quoted (*s.current == '"');

    if (quoted)
    {
      ++s.current; // Skip opening quote.
      ++s.column;
    }

    string r;
    r.reserve (64);

    while (s.current < s.end)
    {
      char c (*s.current);

      if (quoted)
      {
        if (c == '"')
        {
          ++s.current; // Skip closing quote.
          ++s.column;
          break;
        }
        else if (c == '\\' && (s.current + 1) < s.end)
        {
          // Handle escape sequences.
          //
          ++s.current;
          ++s.column;
          char next (*s.current);

          switch (next)
          {
            case 'n':  r += '\n'; break;
            case 't':  r += '\t'; break;
            case 'r':  r += '\r'; break;
            case '\\': r += '\\'; break;
            case '"':  r += '"'; break;
            default:   r += next; break;
          }

          ++s.current;
          ++s.column;
        }
        else
        {
          r += c;
          ++s.current;
          ++s.column;
        }
      }
      else
      {
        // Unquoted strings end at whitespace or special characters.
        //
        if (std::isspace (static_cast<unsigned char> (c)) ||
            c == '{' || c == '}' || c == '"')
        {
          break;
        }

        r += c;
        ++s.current;
        ++s.column;
      }
    }

    return r;
  }

  // Parse a key-value pair.
  //
  // The structure is always "Key" followed by either a "StringValue" or a
  // nested Object { ... }.
  //
  pair<string, vdf_node> vdf_parser::
  parse_pair (parser_state& s)
  {
    // Parse key.
    //
    string key (parse_string (s));

    // Peek ahead to see if we have an object or a simple string value.
    //
    char next (peek_char (s));

    if (next == '{')
    {
      // It's a nested object.
      //
      next_char (s); // Consume '{'.
      auto obj (parse_object (s));

      if (next_char (s) != '}')
      {
        launcher::log::error (categories::steam{}, "expected '}}' at line {}, column {}", s.line, s.column);
        throw runtime_error ("expected '}' at line " + std::to_string (s.line));
      }

      launcher::log::trace_l3 (categories::steam{}, "parsed vdf object pair, key: '{}'", key);
      return {move (key), vdf_node (move (obj))};
    }
    else
    {
      // It's a simple string value.
      //
      string val (parse_string (s));
      launcher::log::trace_l3 (categories::steam{}, "parsed vdf string pair, key: '{}', val: '{}'", key, val);
      return {move (key), vdf_node (move (val))};
    }
  }

  // Parse a map of key-value pairs.
  //
  // We keep parsing pairs until we hit the closing brace or run out of input.
  //
  map<string, vdf_node> vdf_parser::
  parse_object (parser_state& s)
  {
    map<string, vdf_node> r;

    while (true)
    {
      char c (peek_char (s));

      if (c == '\0' || c == '}')
        break;

      auto [key, val] = parse_pair (s);
      r.emplace (move (key), move (val));
    }

    return r;
  }

  // Main entry point for parsing a VDF string.
  //
  vdf_node vdf_parser::
  parse (const string& str)
  {
    launcher::log::trace_l2 (categories::steam{}, "starting vdf string parse (length: {} bytes)", str.size ());
    parser_state s (str);

    // VDF files typically have a single root key, but sometimes (like ACFs)
    // they are just a bare list of pairs.
    //
    char first (peek_char (s));

    if (first == '\0')
    {
      launcher::log::warning (categories::steam{}, "vdf string is empty or contains only whitespace");
      return vdf_node (map<string, vdf_node> {});
    }

    if (first == '{')
    {
      // Direct object (no root key).
      //
      launcher::log::trace_l3 (categories::steam{}, "vdf root is a direct object");
      next_char (s); // Consume '{'.
      auto obj (parse_object (s));
      return vdf_node (move (obj));
    }
    else
    {
      // Parse as a single root pair.
      //
      launcher::log::trace_l3 (categories::steam{}, "vdf root is a single pair");
      auto [key, val] = parse_pair (s);
      map<string, vdf_node> root;
      root.emplace (move (key), move (val));
      return vdf_node (move (root));
    }
  }

  vdf_node vdf_parser::
  parse_file (const fs::path& f)
  {
    launcher::log::trace_l2 (categories::steam{}, "parsing vdf file: {}", f.string ());
    ifstream ifs (f, ios::binary);
    if (!ifs)
    {
      launcher::log::error (categories::steam{}, "failed to open vdf file for parsing: {}", f.string ());
      throw runtime_error ("failed to open file: " + f.string ());
    }

    return parse_stream (ifs);
  }

  vdf_node vdf_parser::
  parse_stream (istream& is)
  {
    ostringstream oss;
    oss << is.rdbuf ();
    return parse (oss.str ());
  }

  // Asynchronous wrappers.
  //
  // Note that for now, we perform the parsing synchronously as VDF files are
  // typically small and memory-mapped parsing isn't strictly necessary.
  //
  asio::awaitable<vdf_node> vdf_parser::
  parse_async (asio::io_context&, const string& str)
  {
    co_return parse (str);
  }

  asio::awaitable<vdf_node> vdf_parser::
  parse_file_async (asio::io_context&, const fs::path& f)
  {
    co_return parse_file (f);
  }

  // Steam-specific parsers.
  //

  // Parse the libraryfolders.vdf file.
  //
  // This file contains the configuration for all Steam library folders on the
  // system. We map the generic VDF nodes into our steam_library struct.
  //
  asio::awaitable<vector<steam_library>>
  parse_library_folders (asio::io_context& ioc, const fs::path& f)
  {
    launcher::log::trace_l1 (categories::steam{}, "parsing library folders from: {}", f.string ());

    vdf_node root (co_await vdf_parser::parse_file_async (ioc, f));
    vector<steam_library> libraries;

    // The structure is typically: "libraryfolders" -> { "0": {...}, "1": {...} }
    //
    const auto* lib_obj (root.get_object ("libraryfolders"));
    if (lib_obj == nullptr)
    {
      launcher::log::warning (categories::steam{}, "no 'libraryfolders' object found in vdf: {}", f.string ());
      co_return libraries;
    }

    for (const auto& [key, node] : *lib_obj)
    {
      // Skip non-numeric keys (metadata fields like "contentstatsid").
      //
      if (key.empty () || !std::isdigit (static_cast<unsigned char> (key[0])))
      {
        launcher::log::trace_l3 (categories::steam{}, "skipping non-numeric libraryfolders key: {}", key);
        continue;
      }

      if (!node.is_object ())
      {
        launcher::log::warning (categories::steam{}, "libraryfolder entry '{}' is not an object", key);
        continue;
      }

      const auto& data (node.as_object ());
      steam_library lib;

      // Extract path.
      //
      if (const auto* n = node.find ("path"); n && n->is_string ())
      {
        fs::path p (n->as_string ());
        lib.path = p.lexically_normal ().make_preferred ();
        launcher::log::trace_l3 (categories::steam{}, "extracted library path: {}", lib.path.string ());
      }

      // Extract label.
      //
      if (const auto* n = node.find ("label"); n && n->is_string ())
      {
        lib.label = n->as_string ();
        launcher::log::trace_l3 (categories::steam{}, "extracted library label: {}", lib.label);
      }

      // Extract contentid.
      //
      if (const auto* n = node.find ("contentid"); n && n->is_string ())
      {
        try
        {
          lib.contentid = std::stoull (n->as_string ());
        }
        catch (const std::exception& e)
        {
          launcher::log::warning (categories::steam{}, "failed to parse contentid for library '{}': {}", key, e.what ());
        }
      }

      // Extract totalsize.
      //
      if (const auto* n = node.find ("totalsize"); n && n->is_string ())
      {
        try
        {
          lib.totalsize = std::stoull (n->as_string ());
        }
        catch (const std::exception& e)
        {
          launcher::log::warning (categories::steam{}, "failed to parse totalsize for library '{}': {}", key, e.what ());
        }
      }

      // Extract apps mapping.
      //
      if (const auto* apps = node.get_object ("apps"))
      {
        for (const auto& [appid, val] : *apps)
        {
          if (val.is_string ())
            lib.apps[appid] = val.as_string ();
        }
        launcher::log::trace_l3 (categories::steam{}, "extracted {} apps for library '{}'", lib.apps.size (), key);
      }

      if (!lib.path.empty ())
        libraries.push_back (move (lib));
      else
        launcher::log::warning (categories::steam{}, "library entry '{}' has an empty path, skipping", key);
    }

    launcher::log::info (categories::steam{}, "parsed {} library folders from vdf", libraries.size ());
    co_return libraries;
  }

  // Parse an app manifest (appmanifest_*.acf).
  //
  // It usually describes the installation state of a single game.
  //
  asio::awaitable<steam_app_manifest>
  parse_app_manifest (asio::io_context& ioc, const fs::path& f)
  {
    launcher::log::trace_l2 (categories::steam{}, "parsing app manifest from: {}", f.string ());

    vdf_node root (co_await vdf_parser::parse_file_async (ioc, f));
    steam_app_manifest m;

    // The structure is rooted under "AppState".
    //
    const auto* state (root.get_object ("AppState"));
    if (state == nullptr)
    {
      launcher::log::warning (categories::steam{}, "no 'AppState' object found in app manifest: {}", f.string ());
      co_return m;
    }

    // Helper to safely extract integer fields.
    //
    auto get_uint = [&state, &f] (const string& k, auto& dest)
    {
      if (const auto n = state->find (k); n != state->end () && n->second.is_string ())
      {
        try
        {
          if constexpr (sizeof (dest) == 8)
            dest = std::stoull (n->second.as_string ());
          else
            dest = std::stoul (n->second.as_string ());
        }
        catch (const std::exception& e)
        {
          launcher::log::warning (categories::steam{}, "failed to parse uint field '{}' in manifest {}: {}", k, f.string (), e.what ());
        }
      }
    };

    get_uint ("appid", m.appid);
    get_uint ("SizeOnDisk", m.size_on_disk);
    get_uint ("buildid", m.buildid);

    try
    {
      m.name = vdf_node (state->at ("AppState")).get_string ("name", "");
      m.installdir = vdf_node (state->at ("AppState")).get_string ("installdir", "");
      m.last_updated = vdf_node (state->at ("AppState")).get_string ("LastUpdated", "");
    }
    catch (const std::exception& e)
    {
      launcher::log::trace_l3 (categories::steam{}, "exception caught accessing 'AppState' nested map fields: {}", e.what ());
    }

    // Direct lookups on the map for strings are a bit verbose, so we just
    // iterate to grab everything else as metadata.
    //
    for (const auto& [key, value] : *state)
    {
      if (value.is_string ())
        m.metadata[key] = value.as_string ();
    }

    launcher::log::debug (categories::steam{}, "parsed app manifest: appid={}, name='{}', installdir='{}'", m.appid, m.name, m.installdir);
    co_return m;
  }

  asio::awaitable<vdf_node>
  parse_config_vdf (asio::io_context& ioc, const fs::path& f)
  {
    launcher::log::trace_l2 (categories::steam{}, "parsing config vdf from: {}", f.string ());
    co_return co_await vdf_parser::parse_file_async (ioc, f);
  }
}
