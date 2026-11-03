// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace launcher
{
  namespace fs = std::filesystem;

  // Kind of manifest, which determines the fields that we parse.
  //
  enum class manifest_format
  {
    update,     // Archives and files.
    dlc         // Files only.
  };

  enum class hash_algorithm
  {
    blake3
  };

  enum class compression_type
  {
    none,
    zip,
    tar_gz,
    tar_bz2
  };

  // Return the hash of the file contents in lower-case hex. Throw
  // std::runtime_error if the file cannot be read.
  //
  std::string
  compute_file_hash (const fs::path& file,
                     hash_algorithm algorithm);

  // Return true if the hashes are equal, ignoring the case.
  //
  bool
  compare_hashes (const std::string& hash1,
                  const std::string& hash2);
}
