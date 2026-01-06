#include <fstream>
#include <sstream>
#include <iomanip>

#include <openssl/evp.h>

namespace hello
{
  template <typename H, typename S>
  bool download_task_traits<H, S>::
  verify (const fs::path& file,
          const string_type& checksum,
          download_verification method)
  {
    if (method == download_verification::none || checksum.empty ())
      return true;

    // Delegate to compute_hash.
    //
    // Note that compute_hash returns an empty string if the file does not exist
    // or if the computation fails (e.g., OpenSSL error).
    //
    string_type h (compute_hash (file, method));
    return !h.empty () && compare_hashes (h, checksum);
  }

  template <typename H, typename S>
  typename download_task_traits<H, S>::string_type
  download_task_traits<H, S>::
  compute_hash (const fs::path& file,
                download_verification method)
  {
    if (method == download_verification::none)
      return string_type ();

    if (!fs::exists (file))
      return string_type ();

    std::ifstream ifs (file, std::ios::binary);
    if (!ifs)
      return string_type ();

    const EVP_MD* md (nullptr);

    switch (method)
    {
      case download_verification::md5:    md = EVP_md5 ();    break;
      case download_verification::sha1:   md = EVP_sha1 ();   break;
      case download_verification::sha256: md = EVP_sha256 (); break;
      case download_verification::sha512: md = EVP_sha512 (); break;
      case download_verification::none:   return string_type ();
    }

    if (!md)
      return string_type ();

    // Create and initialize the OpenSSL digest context.
    //
    EVP_MD_CTX* ctx (EVP_MD_CTX_new ());
    if (!ctx)
      return string_type ();

    if (EVP_DigestInit_ex (ctx, md, nullptr) != 1)
    {
      EVP_MD_CTX_free (ctx);
      return string_type ();
    }

    // Process the file in chunks.
    //
    char buffer [8192];
    while (ifs.read (buffer, sizeof (buffer)) || ifs.gcount () > 0)
    {
      if (EVP_DigestUpdate (ctx,
                            buffer,
                            static_cast<std::size_t> (ifs.gcount ())) != 1)
      {
        EVP_MD_CTX_free (ctx);
        return string_type ();
      }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len (0);

    if (EVP_DigestFinal_ex (ctx, hash, &hash_len) != 1)
    {
      EVP_MD_CTX_free (ctx);
      return string_type ();
    }

    EVP_MD_CTX_free (ctx);

    std::ostringstream oss;
    for (unsigned int i (0); i < hash_len; ++i)
      oss << std::hex << std::setw (2) << std::setfill ('0')
          << static_cast<int> (hash[i]);

    return string_type (oss.str ());
  }

  template <typename H, typename S>
  bool download_task_traits<H, S>::
  compare_hashes (const string_type& hash1,
                  const string_type& hash2)
  {
    if (hash1.size () != hash2.size ())
      return false;

    for (std::size_t i (0); i < hash1.size (); ++i)
    {
      char c1 (std::tolower (hash1[i]));
      char c2 (std::tolower (hash2[i]));
      if (c1 != c2)
        return false;
    }

    return true;
  }
}
