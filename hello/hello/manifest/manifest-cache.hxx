#pragma once

#include <hello/manifest/manifest-types.hxx>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <cstdint>

namespace hello
{
  namespace fs = std::filesystem;

  // Forward declare basic_hash.
  //
  template <typename S>
  struct basic_hash;

  // Archive cache entry.
  //
  template <typename S = std::string, typename H = basic_hash<S>>
  struct basic_manifest_cache_entry
  {
    using string_type = S;
    using hash_type = H;
    using size_type = std::uint64_t;

    // Archive identity.
    //
    string_type archive_name;      // Name of the archive (e.g., "release.zip")
    hash_type archive_hash;        // Hash of the archive file
    size_type archive_size;        // Size of the archive file

    // Extracted files.
    //
    struct extracted_file
    {
      string_type path;            // Relative path where file was extracted
      hash_type hash;              // Hash of the extracted file
      size_type size;              // Size of the extracted file

      bool empty () const noexcept { return path.empty (); }
    };

    std::vector<extracted_file> files;

    // Timestamp of when extraction occurred.
    //
    std::uint64_t timestamp;

    basic_manifest_cache_entry () : archive_size (0), timestamp (0) {}

    bool empty () const noexcept { return archive_name.empty (); }
  };

  // Archive cache manager.
  //
  template <typename S = std::string, typename H = basic_hash<S>>
  class basic_manifest_cache
  {
  public:
    using string_type = S;
    using hash_type = H;
    using entry_type = basic_manifest_cache_entry<S, H>;

  private:
    std::vector<entry_type> entries_;
    fs::path cache_file_;
    bool dirty_;

  public:
    // Constructor.
    //
    explicit basic_manifest_cache (fs::path cache_file)
      : cache_file_ (std::move (cache_file)), dirty_ (false) {}

    // Load cache from disk.
    //
    void load ();

    // Save cache to disk.
    //
    void save () const;

    // Find cache entry for an archive.
    //
    std::optional<entry_type>
    find (const string_type& archive_name,
          const hash_type& archive_hash) const;

    // Add or update cache entry for an archive.
    //
    void add (entry_type entry);

    // Remove cache entry for an archive.
    //
    void remove (const string_type& archive_name);

    // Verify that all files in a cache entry still exist and match their
    // checksums.
    //
    bool verify_entry (const entry_type& entry,
                       const fs::path& install_dir) const;

    // Clear all entries.
    //
    void clear ();

    // Check if cache is dirty (needs saving).
    //
    bool dirty () const noexcept { return dirty_; }

    // Get all entries.
    //
    const std::vector<entry_type>& entries () const noexcept
    {
      return entries_;
    }
  };

  // Common type aliases.
  //
  using archive_cache_entry = basic_manifest_cache_entry<>;
  using archive_cache = basic_manifest_cache<>;
}
