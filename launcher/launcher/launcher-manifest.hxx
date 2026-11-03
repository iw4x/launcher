// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <launcher/manifest/manifest.hxx>

#include <boost/asio.hpp>

#include <string>
#include <filesystem>
#include <vector>
#include <optional>

namespace launcher
{
  namespace fs = std::filesystem;
  namespace asio = boost::asio;

  // Manifest loading and the mapping of its paths to the installation.
  //
  class manifest_coordinator
  {
  public:
    using manifest_type = manifest;
    using file_type = manifest_file;
    using archive_type = manifest_archive;

    // Parse the manifest JSON and link its files to their archives. Throw
    // std::runtime_error if the string is empty and the manifest parsing
    // exceptions if it is invalid.
    //
    static manifest_type
    parse (const std::string& json_str,
           manifest_format kind = manifest_format::update);

    // As above but read the manifest from the file.
    //
    static manifest_type
    load (const fs::path& file,
          manifest_format kind = manifest_format::update);

    // Write the manifest JSON to the file, creating its directory if
    // necessary. Throw std::runtime_error on failures.
    //
    static void
    save (const manifest_type& m,
          const fs::path& file);

    // Validate the manifest (see manifest::validate()).
    //
    static bool
    validate (const manifest_type& m);

    // Return the path in the installation that the manifest file goes to.
    // We map the paths of earlier manifests and the bare file names to the
    // directories that the clients read them from.
    //
    static fs::path
    resolve_path (const file_type& file,
                  const fs::path& install_dir);

    // As above but for the archive.
    //
    static fs::path
    resolve_path (const archive_type& archive,
                  const fs::path& install_dir);

    // Extract the zip archive into the installation, placing each file the
    // same way as resolve_path(), and return the extracted paths. Throw
    // std::runtime_error on failures.
    //
    static asio::awaitable<std::vector<fs::path>>
    extract_archive (const archive_type& archive,
                     const fs::path& archive_path,
                     const fs::path& install_dir);

    // Return the number of files in the manifest, including those in the
    // archives.
    //
    static std::size_t
    get_file_count (const manifest_type& m);

    static bool
    is_empty (const manifest_type& m);
  };
}
