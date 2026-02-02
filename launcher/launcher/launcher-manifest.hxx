#pragma once

#include <launcher/manifest/manifest.hxx>

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <filesystem>
#include <vector>
#include <optional>

namespace launcher
{
  namespace fs = std::filesystem;
  namespace asio = boost::asio;

  class manifest_coordinator
  {
  public:
    using manifest_type = manifest;
    using file_type = manifest_file;
    using archive_type = manifest_archive;

    // Parse manifest from JSON string.
    //
    // Throws if JSON is malformed or manifest structure is invalid.
    //
    static manifest_type
    parse (const std::string& json_str,
           manifest_format kind = manifest_format::update);

    // Load manifest from file.
    //
    // Reads the file, parses JSON, and returns the manifest.
    //
    static manifest_type
    load (const fs::path& file,
          manifest_format kind = manifest_format::update);

    // Save manifest to file.
    //
    // Serializes the manifest to JSON and writes to the specified path.
    //
    static void
    save (const manifest_type& m,
          const fs::path& file);

    // Validate manifest structure.
    //
    // Checks that all required fields are present and constraints are met.
    // Returns true if valid, false otherwise.
    //
    static bool
    validate (const manifest_type& m);

    // Resolve file path.
    //
    // Combines installation directory with the file's relative path.
    //
    static fs::path
    resolve_path (const file_type& file,
                  const fs::path& install_dir);

    // Resolve archive path.
    //
    static fs::path
    resolve_path (const archive_type& archive,
                  const fs::path& install_dir);

    // Extract files from an archive.
    //
    // Given an archive file, extracts its contents to the installation
    // directory according to the file entries in the manifest.
    //
    // Throws if extraction fails or if archive format is unsupported.
    //
    static asio::awaitable<void>
    extract_archive (const archive_type& archive,
                     const fs::path& archive_path,
                     const fs::path& install_dir);

    // Get file count.
    //
    // Returns the total number of files in the manifest (including files
    // within archives).
    //
    static std::size_t
    get_file_count (const manifest_type& m);

    // Check if manifest is empty.
    //
    static bool
    is_empty (const manifest_type& m);
  };
}
