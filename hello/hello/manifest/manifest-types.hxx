#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace hello
{
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
}
