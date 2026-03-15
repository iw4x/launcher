#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <filesystem>

#include <launcher/download/download-types.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // Download request represents a single download operation.
  //
  struct download_request
  {
    // Source URLs (fallback order).
    //
    std::vector<std::string> urls;

    // Target file path.
    //
    fs::path target;

    // Optional size hint.
    //
    std::optional<std::uint64_t> expected_size;

    // Priority.
    //
    download_priority priority {download_priority::normal};

    // Resume support.
    //
    bool resume {true};

    // Timeout settings (in seconds).
    //
    std::uint32_t connect_timeout {30};
    std::uint32_t transfer_timeout {300};

    // Rate limiting (bytes per second, 0 = no limit).
    //
    std::uint64_t rate_limit_bytes_per_second {0};

    // Request metadata.
    //
    std::string name;        // Human-readable name
    std::string description; // Optional description

    // Constructors.
    //
    download_request () = default;

    download_request (std::string url, fs::path tgt);
    download_request (std::vector<std::string> us, fs::path tgt);

    // Validation.
    //
    bool
    valid () const;
  };

  bool
  operator== (const download_request& x, const download_request& y);

  bool
  operator!= (const download_request& x, const download_request& y);
}
