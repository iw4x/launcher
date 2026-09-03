#pragma once

#include <launcher/protobuf/protobuf-wire.hxx>
#include <launcher/steam/steam-crypto.hxx>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace launcher
{
  enum class depot_file_flag : std::uint32_t
  {
    user_config           = 1,
    versioned_user_config = 2,
    encrypted             = 4,
    read_only             = 8,
    hidden                = 16,
    executable            = 32,
    directory             = 64,
    custom_executable     = 128,
    install_script        = 256,
    symlink               = 512
  };

  class steam_manifest_error : public std::runtime_error
  {
  public:
    explicit
    steam_manifest_error (const std::string& what);
  };

  struct steam_depot_chunk
  {
    crypto::sha1_digest id {};

    std::uint32_t checksum = 0;

    std::uint64_t offset = 0;

    std::uint32_t compressed_size   = 0;
    std::uint32_t uncompressed_size = 0;

    std::string
    name () const;
  };

  struct steam_depot_file
  {
    std::string path;

    std::uint64_t size  = 0;
    std::uint32_t flags = 0;

    crypto::sha1_digest content_sha {};

    std::string link_target;

    std::vector<steam_depot_chunk> chunks;

    bool
    has (depot_file_flag f) const noexcept
    {
      return (flags & static_cast<std::uint32_t> (f)) != 0;
    }

    bool
    directory () const noexcept
    {
      return has (depot_file_flag::directory);
    }

    bool
    symlink () const noexcept
    {
      return has (depot_file_flag::symlink);
    }

    bool
    executable () const noexcept
    {
      return has (depot_file_flag::executable);
    }
  };

  struct steam_depot_manifest
  {
    std::uint32_t depot_id      = 0;
    std::uint64_t manifest_id   = 0;
    std::uint32_t creation_time = 0;

    bool filenames_encrypted = false;

    std::uint64_t total_uncompressed = 0;
    std::uint64_t total_compressed   = 0;
    std::uint32_t unique_chunks      = 0;

    std::vector<steam_depot_file> files;

    std::uint64_t
    payload_size () const noexcept;

    std::size_t
    file_count () const noexcept;
  };

  steam_depot_manifest
  parse_depot_manifest (pb::byte_view body, const crypto::aes_key* key);

  steam_depot_manifest
  parse_depot_manifest_archive (pb::byte_view archive,
                                const crypto::aes_key* key);

  std::string
  normalize_manifest_path (std::string_view raw);
}
