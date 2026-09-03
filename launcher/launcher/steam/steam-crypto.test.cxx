#include <launcher/steam/steam-crypto.hxx>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace std;
using namespace launcher;

namespace
{
  using crypto::byte_buffer;
  using crypto::byte_view;

  byte_view
  view (const byte_buffer& b)
  {
    return byte_view (b.data (), b.size ());
  }

  byte_view
  view (const string& s)
  {
    return byte_view (reinterpret_cast<const uint8_t*> (s.data ()),
                      s.size ());
  }

  template <size_t N>
  byte_view
  view (const array<uint8_t, N>& a)
  {
    return byte_view (a.data (), a.size ());
  }

  template <typename F>
  void
  check_throws (F f)
  {
    bool threw (false);

    try
    {
      f ();
    }
    catch (const crypto::crypto_error&)
    {
      threw = true;
    }

    assert (threw);
  }

  void
  test_sha1 ()
  {
    assert (crypto::hex_encode (view (crypto::sha1 (view (string ("abc"))))) ==
            "a9993e364706816aba3e25717850c26c9cd0d89d");

    assert (crypto::hex_encode (view (crypto::sha1 (view (string ())))) ==
            "da39a3ee5e6b4b0d3255bfef95601890afd80709");

    assert (crypto::hex_encode (view (crypto::sha1 (view (
              string ("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnop"
                      "nopq"))))) ==
            "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
  }

  void
  test_crc32 ()
  {
    assert (crypto::crc32_ieee (view (string ("123456789"))) == 0xcbf43926u);
    assert (crypto::crc32_ieee (view (string ())) == 0u);
  }

  void
  test_hex ()
  {
    const uint8_t d[] = {0x00, 0x0f, 0xf0, 0xff};

    assert (crypto::hex_encode (byte_view (d, sizeof (d))) == "000ff0ff");

    byte_buffer r (crypto::hex_decode ("000FF0ff"));

    assert (r.size () == 4);
    assert (r[0] == 0x00 && r[1] == 0x0f && r[2] == 0xf0 && r[3] == 0xff);

    assert (crypto::hex_decode ("").empty ());

    check_throws ([] { crypto::hex_decode ("abc"); });
    check_throws ([] { crypto::hex_decode ("zz"); });
  }

  void
  test_base64 ()
  {
    const pair<const char*, const char*> vs[] = {{"", ""},
                                                 {"f", "Zg=="},
                                                 {"fo", "Zm8="},
                                                 {"foo", "Zm9v"},
                                                 {"foob", "Zm9vYg=="},
                                                 {"fooba", "Zm9vYmE="},
                                                 {"foobar", "Zm9vYmFy"}};

    for (const auto& [plain, encoded] : vs)
    {
      string p (plain);

      assert (crypto::base64_encode (view (p)) == encoded);

      byte_buffer d (crypto::base64_decode (encoded));

      assert (string (d.begin (), d.end ()) == p);
    }

    const uint8_t bin[] = {0xff, 0xfe, 0xfd, 0x00, 0x01};

    string e (crypto::base64_encode (byte_view (bin, sizeof (bin))));
    byte_buffer d (crypto::base64_decode (e));

    assert (d.size () == sizeof (bin));
    assert (equal (d.begin (), d.end (), bin));

    assert (crypto::base64_decode ("Zm9v\nYmFy").size () == 6);

    check_throws ([] { crypto::base64_decode ("Zm9v*mFy"); });
    check_throws ([] { crypto::base64_decode ("Zg==Zg=="); });
    check_throws ([] { crypto::base64_decode ("Zm9vYmF"); });
  }

  const crypto::aes_key test_key = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

  const uint8_t test_ciphertext[] = {
    0xb6, 0xba, 0x33, 0x6c, 0x23, 0x2b, 0x4b, 0xb5, 0xee, 0x6d, 0xc0, 0x67,
    0x24, 0x0e, 0xf9, 0xe0, 0xd9, 0x03, 0x4e, 0x01, 0xbf, 0xf2, 0xaf, 0xac,
    0x04, 0xde, 0x9f, 0x8d, 0x4a, 0xb2, 0xd6, 0x56, 0x69, 0x02, 0x11, 0x88,
    0x5f, 0x5b, 0xab, 0x6f, 0x23, 0x4e, 0xf2, 0x10, 0x7c, 0x48, 0x57, 0x3f,
    0xfa, 0xc2, 0x8d, 0xac, 0xac, 0x9f, 0x90, 0x27, 0x29, 0x1e, 0x34, 0xbb,
    0xb5, 0x28, 0xf1, 0xac};

  const char test_plaintext[] =
    "iw4x depot chunk plaintext, 42 bytes exactly!!";

  const uint8_t test_iv[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
                             0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};

  void
  test_symmetric_decrypt ()
  {
    byte_buffer p (crypto::symmetric_decrypt (
      test_key, byte_view (test_ciphertext, sizeof (test_ciphertext))));

    assert (string (p.begin (), p.end ()) == test_plaintext);

    byte_buffer q (crypto::symmetric_decrypt_with_iv (
      test_key,
      byte_view (test_iv, sizeof (test_iv)),
      byte_view (test_ciphertext + crypto::aes_block_size,
                 sizeof (test_ciphertext) - crypto::aes_block_size)));

    assert (string (q.begin (), q.end ()) == test_plaintext);
  }

  void
  test_symmetric_decrypt_rejects_bad_input ()
  {
    crypto::aes_key wrong (test_key);
    wrong[0] ^= 0xff;

    check_throws ([&wrong]
    {
      crypto::symmetric_decrypt (
        wrong, byte_view (test_ciphertext, sizeof (test_ciphertext)));
    });

    check_throws ([]
    {
      crypto::symmetric_decrypt (
        test_key, byte_view (test_ciphertext, crypto::aes_block_size));
    });

    check_throws ([]
    {
      crypto::symmetric_decrypt (
        test_key, byte_view (test_ciphertext, crypto::aes_block_size + 5));
    });

    check_throws ([]
    {
      crypto::symmetric_decrypt_with_iv (
        test_key,
        byte_view (test_iv, 8),
        byte_view (test_ciphertext, crypto::aes_block_size));
    });
  }

  void
  test_rsa_encrypt ()
  {
    const string mod =
      "c75f453c913f513fc303a0fd42e711b5d981d28854223a53e131828c24c366e5"
      "56fc0461ee4db12288714b985cecd7067e649e41b2c66b22207031a3ecdfee81"
      "6d38f00628e36442a914d2f3ef21491aa48bf1919e57cb475aa0d82ac8bd05c1"
      "9c3ccd1d6b85c110a537bb19c7df24c3d73c0edf604c724f03f0f91d98158a3a"
      "abbc1cb78eff544066f2322e9fcd405736f422e39c4fd72defd65171daec2373"
      "8eeab6b719b800138691178281111112b5db3efe0a04674b3d3b9a1f9f1f9f01"
      "9f11f9c1e9b1d9a1c9b1a9f1e9d1c9b1a919293949596979899a9b9c9d9e9fa0"
      "b1c1d1e1f10203040506070809a0b0c0d0e0f1112131415161718191a1b1c1d3";

    const string exp = "010001";

    string secret ("hunter2");

    byte_buffer a (crypto::rsa_encrypt_pkcs1 (mod, exp, view (secret)));
    byte_buffer b (crypto::rsa_encrypt_pkcs1 (mod, exp, view (secret)));

    assert (a.size () == 256);
    assert (b.size () == 256);
    assert (a != b);

    check_throws ([&exp, &secret]
    {
      crypto::rsa_encrypt_pkcs1 ("not hex", exp, view (secret));
    });

    check_throws ([&mod, &secret]
    {
      crypto::rsa_encrypt_pkcs1 (mod, "", view (secret));
    });
  }

  void
  test_hkdf ()
  {
    string ikm ("machine-id:0123456789abcdef");
    string salt ("iw4x-launcher");

    crypto::aes_key a (crypto::hkdf_sha256 (view (ikm), view (salt), "steam"));
    crypto::aes_key b (crypto::hkdf_sha256 (view (ikm), view (salt), "steam"));

    assert (a == b);
    assert ((a != crypto::aes_key {}));

    string other ("machine-id:fedcba9876543210");
    assert (crypto::hkdf_sha256 (view (other), view (salt), "steam") != a);

    string salt2 ("iw4x-launcher-2");
    assert (crypto::hkdf_sha256 (view (ikm), view (salt2), "steam") != a);
    assert (crypto::hkdf_sha256 (view (ikm), view (salt), "other") != a);

    check_throws ([] { crypto::hkdf_sha256 (byte_view {}, byte_view {}, "x"); });
  }

  void
  test_aead ()
  {
    crypto::aes_key k (crypto::hkdf_sha256 (
      view (string ("secret")), view (string ("salt")), "aead-test"));

    string p ("{\"refresh_token\":\"eyJ...\"}");
    string aad ("iw4x-launcher/steam/v1");

    crypto::byte_buffer s (
      crypto::aead_encrypt (k, view (p), view (aad)));

    assert (s.size () ==
            crypto::aead_nonce_size + p.size () + crypto::aead_tag_size);

    crypto::byte_buffer r (crypto::aead_decrypt (k, view (s), view (aad)));
    assert (string (r.begin (), r.end ()) == p);

    assert (crypto::aead_encrypt (k, view (p), view (aad)) != s);

    for (size_t i : {size_t (0),
                     crypto::aead_nonce_size + 1,
                     s.size () - 1})
    {
      crypto::byte_buffer t (s);
      t[i] ^= 0xff;

      check_throws ([&k, &t, &aad]
      {
        crypto::aead_decrypt (k, view (t), view (aad));
      });
    }

    crypto::aes_key k2 (k);
    k2[0] ^= 0xff;

    check_throws ([&k2, &s, &aad]
    {
      crypto::aead_decrypt (k2, view (s), view (aad));
    });

    check_throws ([&k, &s]
    {
      string other ("iw4x-launcher/steam/v2");
      crypto::aead_decrypt (k, view (s), view (other));
    });

    check_throws ([&k, &aad]
    {
      crypto::byte_buffer t (crypto::aead_nonce_size);
      crypto::aead_decrypt (k, view (t), view (aad));
    });
  }

  void
  test_random_and_wipe ()
  {
    array<uint8_t, 32> a {};
    array<uint8_t, 32> b {};

    crypto::random_bytes (a);
    crypto::random_bytes (b);

    assert (a != b);
    assert ((a != array<uint8_t, 32> {}));

    crypto::secure_zero (a);
    assert ((a == array<uint8_t, 32> {}));

    string s ("password");
    crypto::secure_zero (s);
    assert (s.empty ());
  }
}

int
main ()
{
  test_sha1 ();
  test_crc32 ();
  test_hex ();
  test_base64 ();
  test_symmetric_decrypt ();
  test_symmetric_decrypt_rejects_bad_input ();
  test_rsa_encrypt ();
  test_hkdf ();
  test_aead ();
  test_random_and_wipe ();
}
