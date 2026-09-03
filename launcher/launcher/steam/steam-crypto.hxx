#pragma once

#include <launcher/protobuf/protobuf-wire.hxx>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace launcher
{
  namespace crypto
  {
    using pb::byte_buffer;
    using pb::byte_view;

    inline constexpr std::size_t aes_key_size   = 32;
    inline constexpr std::size_t aes_block_size = 16;

    using aes_key     = std::array<std::uint8_t, aes_key_size>;
    using sha1_digest = std::array<std::uint8_t, 20>;

    class crypto_error : public std::runtime_error
    {
    public:
      explicit
      crypto_error (const std::string& what);

      static crypto_error
      from_openssl (const std::string& what);
    };

    void
    random_bytes (std::span<std::uint8_t> out);

    sha1_digest
    sha1 (byte_view);

    class sha1_hasher
    {
    public:
      sha1_hasher ();

      sha1_hasher (const sha1_hasher&) = delete;
      sha1_hasher& operator= (const sha1_hasher&) = delete;

      void
      update (byte_view);

      sha1_digest
      finish ();

    private:
      struct context;

      std::shared_ptr<context> ctx_;
    };

    std::uint32_t
    crc32_ieee (byte_view);

    byte_buffer
    symmetric_decrypt (const aes_key& key, byte_view ciphertext);

    byte_buffer
    symmetric_decrypt_with_iv (const aes_key& key,
                               byte_view iv,
                               byte_view ciphertext);

    byte_buffer
    rsa_encrypt_pkcs1 (std::string_view modulus_hex,
                       std::string_view exponent_hex,
                       byte_view plaintext);

    std::string
    base64_encode (byte_view);

    byte_buffer
    base64_decode (std::string_view);

    std::string
    hex_encode (byte_view);

    byte_buffer
    hex_decode (std::string_view);

    aes_key
    hkdf_sha256 (byte_view ikm, byte_view salt, std::string_view info);

    byte_buffer
    aead_encrypt (const aes_key& key, byte_view plaintext, byte_view aad);

    byte_buffer
    aead_decrypt (const aes_key& key, byte_view sealed, byte_view aad);

    inline constexpr std::size_t aead_nonce_size = 12;
    inline constexpr std::size_t aead_tag_size   = 16;

    void
    secure_zero (std::span<std::uint8_t> buffer) noexcept;

    void
    secure_zero (std::string& s) noexcept;
  }
}
