#pragma once

#include <launcher/protobuf/protobuf-wire.hxx>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace launcher
{
  namespace compression
  {
    using pb::byte_buffer;
    using pb::byte_view;

    class compression_error : public std::runtime_error
    {
    public:
      explicit
      compression_error (const std::string& what);
    };

    byte_buffer
    gzip_decompress (byte_view, std::size_t expected_size);

    byte_buffer
    vzip_decompress (byte_view, bool verify_checksum = true);

    byte_buffer
    vzstd_decompress (byte_view, bool verify_checksum = true);

    byte_buffer
    zip_decompress (byte_view, bool verify_checksum = true);

    byte_buffer
    decompress_depot_chunk (byte_view, std::size_t expected_size);

    std::uint32_t
    adler32_steam (byte_view) noexcept;
  }
}
