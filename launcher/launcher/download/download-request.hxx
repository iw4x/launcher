// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <optional>
#include <filesystem>

#include <launcher/download/download-types.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  // Request to download a file.
  //
  struct download_request
  {
    // URLs to download from, tried in order until one succeeds.
    //
    std::vector<std::string> urls;

    fs::path target;

    // Size of the file if known, which lets us report the progress of a
    // resumed download before the server tells us the size.
    //
    std::optional<std::uint64_t> expected_size;

    download_priority priority {download_priority::normal};

    // Whether to resume from a partial target that already exists.
    //
    bool resume {true};

    std::chrono::seconds connect_timeout {30};
    std::chrono::seconds transfer_timeout {300};

    // Rate limit in bytes per second, 0 for none.
    //
    std::uint64_t rate_limit_bytes_per_second {0};

    // @@ Callers set the name but nothing reads it or the description.
    //
    std::string name;
    std::string description;

    download_request () = default;

    download_request (std::string url, fs::path tgt);
    download_request (std::vector<std::string> us, fs::path tgt);

    // Return true if there is a URL to download from and a target.
    //
    bool
    valid () const;
  };

  bool
  operator== (const download_request& x, const download_request& y);
}
