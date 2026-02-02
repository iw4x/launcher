#pragma once

#include <launcher/manifest/manifest.hxx>
#include <launcher/manifest/manifest-cache.hxx>

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

    // Get files that need to be downloaded.
    //
    // Compares manifest files against the installation directory and returns
    // a list of files that are missing, have incorrect size, or failed
    // verification.
    //
    // This does NOT verify hashes by default (expensive); it only checks
    // existence and size. Set verify_hashes to true for full verification.
    //
    static std::vector<file_type>
    get_missing_files (const manifest_type& m,
                       const fs::path& install_dir,
                       bool verify_hashes = false);

    // Get archives that need to be downloaded.
    //
    // Returns archives that are missing or have incorrect size in the
    // installation directory. Checks the archive cache to avoid redownloading
    // archives whose content has already been extracted.
    //
    static std::vector<archive_type>
    get_missing_archives (const manifest_type& m,
                          const fs::path& install_dir,
                          archive_cache* cache = nullptr,
                          bool verify_hashes = false);

    // Verify file against manifest entry.
    //
    // Checks that the file exists, has correct size, and matches the hash
    // (if verification is enabled).
    //
    // Returns true if the file is valid.
    //
    static bool
    verify_file (const file_type& file,
                 const fs::path& install_dir,
                 bool verify_hash = true);

    // Verify archive against manifest entry.
    //
    // Checks size and hash of the archive file.
    //
    static bool
    verify_archive (const archive_type& archive,
                    const fs::path& install_dir,
                    bool verify_hash = true);

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
    // directory according to the file entries in the manifest. If a cache
    // pointer is provided, updates the cache with extracted file metadata.
    //
    // Throws if extraction fails or if archive format is unsupported.
    //
    static asio::awaitable<void>
    extract_archive (const archive_type& archive,
                     const fs::path& archive_path,
                     const fs::path& install_dir,
                     archive_cache* cache = nullptr);

    // Get total download size.
    //
    // Calculates the total number of bytes that need to be downloaded based
    // on missing files and archives.
    //
    static std::uint64_t
    calculate_download_size (const manifest_type& m,
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

  // Compute hash of data buffer.
  //
  // Utility function to compute hash using the manifest's hash algorithm.
  //
  std::string
  compute_hash (const void* data,
                std::size_t size,
                hash_algorithm algorithm);
}
