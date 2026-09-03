#include <launcher/steam/steam-crypto.hxx>

#include <cassert>
#include <cctype>
#include <memory>

#include <miniz.h>

#undef crc32
#undef compress
#undef uncompress

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/rsa.h>

using namespace std;

namespace launcher
{
  namespace crypto
  {
    namespace
    {
      struct evp_cipher_ctx_deleter
      {
        void
        operator() (EVP_CIPHER_CTX* p) const noexcept
        {
          EVP_CIPHER_CTX_free (p);
        }
      };

      struct evp_md_ctx_deleter
      {
        void
        operator() (EVP_MD_CTX* p) const noexcept
        {
          EVP_MD_CTX_free (p);
        }
      };

      struct evp_pkey_deleter
      {
        void
        operator() (EVP_PKEY* p) const noexcept
        {
          EVP_PKEY_free (p);
        }
      };

      struct evp_pkey_ctx_deleter
      {
        void
        operator() (EVP_PKEY_CTX* p) const noexcept
        {
          EVP_PKEY_CTX_free (p);
        }
      };

      struct bn_deleter
      {
        void
        operator() (BIGNUM* p) const noexcept
        {
          BN_free (p);
        }
      };

      struct param_bld_deleter
      {
        void
        operator() (OSSL_PARAM_BLD* p) const noexcept
        {
          OSSL_PARAM_BLD_free (p);
        }
      };

      struct param_deleter
      {
        void
        operator() (OSSL_PARAM* p) const noexcept
        {
          OSSL_PARAM_free (p);
        }
      };

      using cipher_ctx = unique_ptr<EVP_CIPHER_CTX, evp_cipher_ctx_deleter>;
      using md_ctx     = unique_ptr<EVP_MD_CTX, evp_md_ctx_deleter>;
      using pkey       = unique_ptr<EVP_PKEY, evp_pkey_deleter>;
      using pkey_ctx   = unique_ptr<EVP_PKEY_CTX, evp_pkey_ctx_deleter>;
      using bignum     = unique_ptr<BIGNUM, bn_deleter>;
      using param_bld  = unique_ptr<OSSL_PARAM_BLD, param_bld_deleter>;
      using params     = unique_ptr<OSSL_PARAM, param_deleter>;
    }

    crypto_error::
    crypto_error (const string& w)
      : runtime_error (w)
    {
    }

    crypto_error crypto_error::
    from_openssl (const string& w)
    {
      string m (w);

      bool first (true);

      for (unsigned long e (ERR_get_error ()); e != 0; e = ERR_get_error ())
      {
        char b[256];
        ERR_error_string_n (e, b, sizeof (b));

        m += first ? ": " : "; ";
        m += b;

        first = false;
      }

      if (first)
        m += ": no additional detail from openssl";

      return crypto_error (m);
    }

    void
    random_bytes (span<uint8_t> out)
    {
      if (out.empty ())
        return;

      if (RAND_bytes (out.data (), static_cast<int> (out.size ())) != 1)
        throw crypto_error::from_openssl ("failed to generate random bytes");
    }

    void
    secure_zero (span<uint8_t> b) noexcept
    {
      if (!b.empty ())
        OPENSSL_cleanse (b.data (), b.size ());
    }

    void
    secure_zero (string& s) noexcept
    {
      if (!s.empty ())
        OPENSSL_cleanse (s.data (), s.size ());

      s.clear ();
    }

    sha1_digest
    sha1 (byte_view d)
    {
      md_ctx c (EVP_MD_CTX_new ());

      if (!c)
        throw crypto_error::from_openssl ("failed to allocate digest context");

      if (EVP_DigestInit_ex (c.get (), EVP_sha1 (), nullptr) != 1)
        throw crypto_error::from_openssl ("failed to initialize sha-1");

      if (!d.empty () &&
          EVP_DigestUpdate (c.get (), d.data (), d.size ()) != 1)
        throw crypto_error::from_openssl ("failed to update sha-1");

      sha1_digest r {};
      unsigned int n (0);

      if (EVP_DigestFinal_ex (c.get (), r.data (), &n) != 1)
        throw crypto_error::from_openssl ("failed to finalize sha-1");

      if (n != r.size ())
        throw crypto_error ("sha-1 produced " + std::to_string (n) +
                            " bytes, expected " + std::to_string (r.size ()));

      return r;
    }

    struct sha1_hasher::context
    {
      md_ctx md;
    };

    sha1_hasher::
    sha1_hasher ()
      : ctx_ (make_shared<context> ())
    {
      ctx_->md.reset (EVP_MD_CTX_new ());

      if (!ctx_->md)
        throw crypto_error::from_openssl ("failed to allocate digest "
                                          "context");

      if (EVP_DigestInit_ex (ctx_->md.get (), EVP_sha1 (), nullptr) != 1)
        throw crypto_error::from_openssl ("failed to initialize sha-1");
    }

    void sha1_hasher::
    update (byte_view d)
    {
      assert (ctx_ && ctx_->md && "hasher used after finish()");

      if (d.empty ())
        return;

      if (EVP_DigestUpdate (ctx_->md.get (), d.data (), d.size ()) != 1)
        throw crypto_error::from_openssl ("failed to update sha-1");
    }

    sha1_digest sha1_hasher::
    finish ()
    {
      assert (ctx_ && ctx_->md && "hasher finished twice");

      sha1_digest  r {};
      unsigned int n (0);

      if (EVP_DigestFinal_ex (ctx_->md.get (), r.data (), &n) != 1)
        throw crypto_error::from_openssl ("failed to finalize sha-1");

      if (n != r.size ())
        throw crypto_error ("sha-1 produced " + std::to_string (n) +
                            " bytes, expected " + std::to_string (r.size ()));

      ctx_->md.reset ();

      return r;
    }

    uint32_t
    crc32_ieee (byte_view d)
    {
      return static_cast<uint32_t> (
        mz_crc32 (MZ_CRC32_INIT, d.data (), d.size ()));
    }

    byte_buffer
    symmetric_decrypt_with_iv (const aes_key& k, byte_view iv, byte_view ct)
    {
      if (iv.size () != aes_block_size)
        throw crypto_error ("initialization vector is " +
                            std::to_string (iv.size ()) +
                            " bytes, expected " +
                            std::to_string (aes_block_size));

      if (ct.empty ())
        throw crypto_error ("ciphertext is empty");

      if (ct.size () % aes_block_size != 0)
        throw crypto_error ("ciphertext length " + std::to_string (ct.size ()) +
                            " is not a multiple of the aes block size");

      cipher_ctx c (EVP_CIPHER_CTX_new ());

      if (!c)
        throw crypto_error::from_openssl ("failed to allocate cipher context");

      if (EVP_DecryptInit_ex (
            c.get (), EVP_aes_256_cbc (), nullptr, k.data (), iv.data ()) != 1)
        throw crypto_error::from_openssl (
          "failed to initialize aes-256-cbc decryption");

      byte_buffer r (ct.size () + aes_block_size);
      int n (0);

      if (EVP_DecryptUpdate (c.get (),
                             r.data (),
                             &n,
                             ct.data (),
                             static_cast<int> (ct.size ())) != 1)
        throw crypto_error::from_openssl ("failed to decrypt payload");

      assert (n >= 0 && static_cast<size_t> (n) <= r.size ());

      int f (0);

      if (EVP_DecryptFinal_ex (c.get (), r.data () + n, &f) != 1)
        throw crypto_error::from_openssl (
          "failed to finalize decryption (wrong key or corrupt payload)");

      assert (f >= 0);

      r.resize (static_cast<size_t> (n) + static_cast<size_t> (f));

      return r;
    }

    byte_buffer
    symmetric_decrypt (const aes_key& k, byte_view ct)
    {
      if (ct.size () <= aes_block_size)
        throw crypto_error ("payload is " + std::to_string (ct.size ()) +
                            " bytes, too short to carry an encrypted "
                            "initialization vector and a ciphertext");

      cipher_ctx c (EVP_CIPHER_CTX_new ());

      if (!c)
        throw crypto_error::from_openssl ("failed to allocate cipher context");

      if (EVP_DecryptInit_ex (
            c.get (), EVP_aes_256_ecb (), nullptr, k.data (), nullptr) != 1)
        throw crypto_error::from_openssl (
          "failed to initialize aes-256-ecb decryption");

      if (EVP_CIPHER_CTX_set_padding (c.get (), 0) != 1)
        throw crypto_error::from_openssl ("failed to disable ecb padding");

      array<uint8_t, aes_block_size> iv {};
      int n (0);

      if (EVP_DecryptUpdate (c.get (),
                             iv.data (),
                             &n,
                             ct.data (),
                             static_cast<int> (aes_block_size)) != 1)
        throw crypto_error::from_openssl (
          "failed to decrypt the initialization vector");

      int f (0);

      if (EVP_DecryptFinal_ex (c.get (), iv.data () + n, &f) != 1)
        throw crypto_error::from_openssl (
          "failed to finalize initialization vector decryption");

      if (static_cast<size_t> (n) + static_cast<size_t> (f) !=
          aes_block_size)
        throw crypto_error ("decrypted initialization vector is " +
                            std::to_string (n + f) + " bytes, expected " +
                            std::to_string (aes_block_size));

      byte_buffer r (symmetric_decrypt_with_iv (
        k, byte_view (iv.data (), iv.size ()), ct.subspan (aes_block_size)));

      secure_zero (span<uint8_t> (iv));

      return r;
    }

    byte_buffer
    rsa_encrypt_pkcs1 (string_view mod_hex, string_view exp_hex, byte_view pt)
    {
      if (mod_hex.empty () || exp_hex.empty ())
        throw crypto_error ("rsa public key is missing its modulus or "
                            "exponent");

      string mh (mod_hex);
      string eh (exp_hex);

      BIGNUM* mp (nullptr);
      BIGNUM* ep (nullptr);

      if (BN_hex2bn (&mp, mh.c_str ()) == 0)
      {
        BN_free (mp);
        throw crypto_error ("rsa modulus is not valid hexadecimal");
      }

      bignum m (mp);

      if (BN_hex2bn (&ep, eh.c_str ()) == 0)
      {
        BN_free (ep);
        throw crypto_error ("rsa exponent is not valid hexadecimal");
      }

      bignum e (ep);

      param_bld b (OSSL_PARAM_BLD_new ());

      if (!b)
        throw crypto_error::from_openssl (
          "failed to allocate a parameter builder");

      if (OSSL_PARAM_BLD_push_BN (b.get (), OSSL_PKEY_PARAM_RSA_N,
                                  m.get ()) != 1 ||
          OSSL_PARAM_BLD_push_BN (b.get (), OSSL_PKEY_PARAM_RSA_E,
                                  e.get ()) != 1)
        throw crypto_error::from_openssl (
          "failed to assemble the rsa public key parameters");

      params ps (OSSL_PARAM_BLD_to_param (b.get ()));

      if (!ps)
        throw crypto_error::from_openssl (
          "failed to build the rsa public key parameters");

      pkey_ctx bc (EVP_PKEY_CTX_new_from_name (nullptr, "RSA", nullptr));

      if (!bc)
        throw crypto_error::from_openssl ("failed to allocate an rsa context");

      if (EVP_PKEY_fromdata_init (bc.get ()) != 1)
        throw crypto_error::from_openssl (
          "failed to initialize rsa key import");

      EVP_PKEY* kp (nullptr);

      if (EVP_PKEY_fromdata (
            bc.get (), &kp, EVP_PKEY_PUBLIC_KEY, ps.get ()) != 1)
        throw crypto_error::from_openssl ("failed to import the rsa public "
                                          "key");

      pkey k (kp);

      pkey_ctx ec (EVP_PKEY_CTX_new_from_pkey (nullptr, k.get (), nullptr));

      if (!ec)
        throw crypto_error::from_openssl (
          "failed to allocate an rsa encryption context");

      if (EVP_PKEY_encrypt_init (ec.get ()) != 1)
        throw crypto_error::from_openssl (
          "failed to initialize rsa encryption");

      if (EVP_PKEY_CTX_set_rsa_padding (ec.get (), RSA_PKCS1_PADDING) != 1)
        throw crypto_error::from_openssl (
          "failed to select pkcs#1 v1.5 padding");

      size_t n (0);

      if (EVP_PKEY_encrypt (
            ec.get (), nullptr, &n, pt.data (), pt.size ()) != 1)
        throw crypto_error::from_openssl (
          "failed to determine the rsa ciphertext size");

      byte_buffer r (n);

      if (EVP_PKEY_encrypt (
            ec.get (), r.data (), &n, pt.data (), pt.size ()) != 1)
        throw crypto_error::from_openssl ("failed to rsa-encrypt the payload");

      assert (n <= r.size ());
      r.resize (n);

      return r;
    }

    namespace
    {
      constexpr char base64_alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

      const array<uint8_t, 256>&
      base64_reverse ()
      {
        static const array<uint8_t, 256> t ([] ()
        {
          array<uint8_t, 256> r {};
          r.fill (0xff);

          for (uint8_t i (0); i != 64; ++i)
            r[static_cast<unsigned char> (base64_alphabet[i])] = i;

          return r;
        } ());

        return t;
      }
    }

    string
    base64_encode (byte_view d)
    {
      string r;
      r.reserve (((d.size () + 2) / 3) * 4);

      size_t i (0);

      for (; i + 3 <= d.size (); i += 3)
      {
        uint32_t v (static_cast<uint32_t> (d[i]) << 16 |
                    static_cast<uint32_t> (d[i + 1]) << 8 |
                    static_cast<uint32_t> (d[i + 2]));

        r += base64_alphabet[(v >> 18) & 0x3f];
        r += base64_alphabet[(v >> 12) & 0x3f];
        r += base64_alphabet[(v >> 6) & 0x3f];
        r += base64_alphabet[v & 0x3f];
      }

      if (size_t t (d.size () - i); t != 0)
      {
        assert (t == 1 || t == 2);

        uint32_t v (static_cast<uint32_t> (d[i]) << 16);

        if (t == 2)
          v |= static_cast<uint32_t> (d[i + 1]) << 8;

        r += base64_alphabet[(v >> 18) & 0x3f];
        r += base64_alphabet[(v >> 12) & 0x3f];
        r += t == 2 ? base64_alphabet[(v >> 6) & 0x3f] : '=';
        r += '=';
      }

      return r;
    }

    byte_buffer
    base64_decode (string_view s)
    {
      const array<uint8_t, 256>& t (base64_reverse ());

      byte_buffer r;
      r.reserve ((s.size () / 4) * 3);

      uint32_t acc (0);
      int      bits (0);
      size_t   pad (0);

      for (size_t i (0); i != s.size (); ++i)
      {
        char c (s[i]);

        if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
          continue;

        if (c == '=')
        {
          if (++pad > 2)
            throw crypto_error ("base64 input has more than two padding "
                                "characters");
          continue;
        }

        if (pad != 0)
          throw crypto_error ("base64 input has data after its padding");

        uint8_t v (t[static_cast<unsigned char> (c)]);

        if (v == 0xff)
          throw crypto_error ("base64 input contains an invalid character at "
                              "offset " + std::to_string (i));

        acc = (acc << 6) | v;
        bits += 6;

        if (bits >= 8)
        {
          bits -= 8;
          r.push_back (static_cast<uint8_t> ((acc >> bits) & 0xff));
        }
      }

      if (bits >= 6 || (acc & ((1u << bits) - 1)) != 0)
        throw crypto_error ("base64 input is truncated");

      return r;
    }

    aes_key
    hkdf_sha256 (byte_view ikm, byte_view salt, string_view info)
    {
      if (ikm.empty ())
        throw crypto_error ("hkdf input keying material is empty");

      EVP_KDF* k (EVP_KDF_fetch (nullptr, "HKDF", nullptr));

      if (k == nullptr)
        throw crypto_error::from_openssl ("failed to fetch the hkdf "
                                          "implementation");

      EVP_KDF_CTX* c (EVP_KDF_CTX_new (k));

      EVP_KDF_free (k);

      if (c == nullptr)
        throw crypto_error::from_openssl ("failed to allocate an hkdf "
                                          "context");

      struct guard
      {
        EVP_KDF_CTX* c;

        ~guard ()
        {
          EVP_KDF_CTX_free (c);
        }
      } g {c};

      char digest[] = "SHA256";

      OSSL_PARAM ps[] = {
        OSSL_PARAM_construct_utf8_string (OSSL_KDF_PARAM_DIGEST,
                                          digest,
                                          0),
        OSSL_PARAM_construct_octet_string (
          OSSL_KDF_PARAM_KEY,
          const_cast<uint8_t*> (ikm.data ()),
          ikm.size ()),
        OSSL_PARAM_construct_octet_string (
          OSSL_KDF_PARAM_SALT,
          const_cast<uint8_t*> (salt.data ()),
          salt.size ()),
        OSSL_PARAM_construct_octet_string (
          OSSL_KDF_PARAM_INFO,
          const_cast<char*> (info.data ()),
          info.size ()),
        OSSL_PARAM_construct_end ()};

      aes_key r {};

      if (EVP_KDF_derive (c, r.data (), r.size (), ps) != 1)
        throw crypto_error::from_openssl ("failed to derive a key with "
                                          "hkdf");

      return r;
    }

    byte_buffer
    aead_encrypt (const aes_key& k, byte_view pt, byte_view aad)
    {
      cipher_ctx c (EVP_CIPHER_CTX_new ());

      if (!c)
        throw crypto_error::from_openssl ("failed to allocate cipher "
                                          "context");

      byte_buffer r (aead_nonce_size + pt.size () + aead_tag_size);

      random_bytes (span<uint8_t> (r.data (), aead_nonce_size));

      if (EVP_EncryptInit_ex (
            c.get (), EVP_aes_256_gcm (), nullptr, nullptr, nullptr) != 1)
        throw crypto_error::from_openssl ("failed to initialize aes-256-gcm");

      if (EVP_CIPHER_CTX_ctrl (c.get (),
                               EVP_CTRL_AEAD_SET_IVLEN,
                               static_cast<int> (aead_nonce_size),
                               nullptr) != 1)
        throw crypto_error::from_openssl ("failed to set the gcm nonce "
                                          "length");

      if (EVP_EncryptInit_ex (
            c.get (), nullptr, nullptr, k.data (), r.data ()) != 1)
        throw crypto_error::from_openssl ("failed to set the gcm key and "
                                          "nonce");

      int n (0);

      if (!aad.empty () &&
          EVP_EncryptUpdate (c.get (),
                             nullptr,
                             &n,
                             aad.data (),
                             static_cast<int> (aad.size ())) != 1)
        throw crypto_error::from_openssl ("failed to authenticate the "
                                          "associated data");

      if (!pt.empty () &&
          EVP_EncryptUpdate (c.get (),
                             r.data () + aead_nonce_size,
                             &n,
                             pt.data (),
                             static_cast<int> (pt.size ())) != 1)
        throw crypto_error::from_openssl ("failed to encrypt the payload");

      assert (n >= 0 && static_cast<size_t> (n) == pt.size ());

      int f (0);

      if (EVP_EncryptFinal_ex (
            c.get (), r.data () + aead_nonce_size + n, &f) != 1)
        throw crypto_error::from_openssl ("failed to finalize encryption");

      assert (f == 0);

      if (EVP_CIPHER_CTX_ctrl (c.get (),
                               EVP_CTRL_AEAD_GET_TAG,
                               static_cast<int> (aead_tag_size),
                               r.data () + aead_nonce_size + pt.size ()) != 1)
        throw crypto_error::from_openssl ("failed to read the "
                                          "authentication tag");

      return r;
    }

    byte_buffer
    aead_decrypt (const aes_key& k, byte_view s, byte_view aad)
    {
      constexpr size_t overhead (aead_nonce_size + aead_tag_size);

      if (s.size () < overhead)
        throw crypto_error ("sealed payload is " + std::to_string (s.size ()) +
                            " bytes, too short to carry a nonce and a tag");

      size_t n (s.size () - overhead);

      cipher_ctx c (EVP_CIPHER_CTX_new ());

      if (!c)
        throw crypto_error::from_openssl ("failed to allocate cipher "
                                          "context");

      if (EVP_DecryptInit_ex (
            c.get (), EVP_aes_256_gcm (), nullptr, nullptr, nullptr) != 1)
        throw crypto_error::from_openssl ("failed to initialize aes-256-gcm");

      if (EVP_CIPHER_CTX_ctrl (c.get (),
                               EVP_CTRL_AEAD_SET_IVLEN,
                               static_cast<int> (aead_nonce_size),
                               nullptr) != 1)
        throw crypto_error::from_openssl ("failed to set the gcm nonce "
                                          "length");

      if (EVP_DecryptInit_ex (
            c.get (), nullptr, nullptr, k.data (), s.data ()) != 1)
        throw crypto_error::from_openssl ("failed to set the gcm key and "
                                          "nonce");

      int m (0);

      if (!aad.empty () &&
          EVP_DecryptUpdate (c.get (),
                             nullptr,
                             &m,
                             aad.data (),
                             static_cast<int> (aad.size ())) != 1)
        throw crypto_error::from_openssl ("failed to authenticate the "
                                          "associated data");

      byte_buffer r (n);

      if (n != 0 &&
          EVP_DecryptUpdate (c.get (),
                             r.data (),
                             &m,
                             s.data () + aead_nonce_size,
                             static_cast<int> (n)) != 1)
        throw crypto_error::from_openssl ("failed to decrypt the payload");

      if (EVP_CIPHER_CTX_ctrl (
            c.get (),
            EVP_CTRL_AEAD_SET_TAG,
            static_cast<int> (aead_tag_size),
            const_cast<uint8_t*> (s.data () + aead_nonce_size + n)) != 1)
        throw crypto_error::from_openssl ("failed to set the authentication "
                                          "tag");

      int f (0);

      if (EVP_DecryptFinal_ex (c.get (), r.data () + m, &f) != 1)
        throw crypto_error (
          "the sealed payload failed authentication: it was produced on a "
          "different machine, under a different user, or has been modified");

      assert (f == 0);

      return r;
    }

    string
    hex_encode (byte_view d)
    {
      constexpr char digits[] = "0123456789abcdef";

      string r;
      r.resize (d.size () * 2);

      for (size_t i (0); i != d.size (); ++i)
      {
        r[i * 2]     = digits[(d[i] >> 4) & 0x0f];
        r[i * 2 + 1] = digits[d[i] & 0x0f];
      }

      return r;
    }

    byte_buffer
    hex_decode (string_view s)
    {
      if (s.size () % 2 != 0)
        throw crypto_error ("hex input has an odd length (" +
                            std::to_string (s.size ()) + ")");

      auto nibble ([&s] (size_t i) -> uint8_t
      {
        char c (s[i]);

        if (c >= '0' && c <= '9') return static_cast<uint8_t> (c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t> (c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t> (c - 'A' + 10);

        throw crypto_error ("hex input contains an invalid character at "
                            "offset " + std::to_string (i));
      });

      byte_buffer r;
      r.reserve (s.size () / 2);

      for (size_t i (0); i != s.size (); i += 2)
        r.push_back (static_cast<uint8_t> ((nibble (i) << 4) | nibble (i + 1)));

      return r;
    }
  }
}
