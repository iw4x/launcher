#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace launcher
{
  namespace fs = std::filesystem;

  // Manifest format kinds.
  //
  enum class manifest_format
  {
    update,     // Update manifest (update.json)
    dlc         // DLC manifest
  };

  // Hash algorithm types.
  //
  enum class hash_algorithm
  {
    blake3
  };

  // File compression types.
  //
  enum class compression_type
  {
    none,
    zip,
    tar_gz,
    tar_bz2
  };

  // Compute hash of file.
  //
  // Reads the entire file and computes its hash.
  //
  std::string
  compute_file_hash (const fs::path& file,
                     hash_algorithm algorithm);

  // Compare two hashes (case-insensitive).
  //
  bool
  compare_hashes (const std::string& hash1,
                  const std::string& hash2);
}
