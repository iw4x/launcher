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
  template <typename S = std::string>
  struct basic_download_request
  {
    using string_type = S;

    // Source URLs (fallback order).
    //
    std::vector<string_type> urls;

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
    string_type name;        // Human-readable name
    string_type description; // Optional description

    // Constructors.
    //
    basic_download_request () = default;

    basic_download_request (string_type url, fs::path tgt)
        : urls ({std::move (url)}),
          target (std::move (tgt))
    {
    }

    basic_download_request (std::vector<string_type> us, fs::path tgt)
      : urls (std::move (us)),
        target (std::move (tgt))
    {
    }

    // Validation.
    //
    bool
    valid () const
    {
      return !urls.empty () && !target.empty ();
    }
  };

  using download_request = basic_download_request<std::string>;

  template <typename S>
  inline bool
  operator== (const basic_download_request<S>& x,
              const basic_download_request<S>& y)
  {
    return x.urls == y.urls &&
           x.target == y.target;
  }

  template <typename S>
  inline bool
  operator!= (const basic_download_request<S>& x,
              const basic_download_request<S>& y)
  {
    return !(x == y);
  }
}
