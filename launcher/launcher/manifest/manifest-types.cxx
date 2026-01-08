#include <launcher/manifest/manifest-types.hxx>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <launcher/blake3.h>

using namespace std;

namespace launcher
{
  string
  compute_file_hash (const fs::path& p, hash_algorithm a)
  {
    if (a != hash_algorithm::blake3)
      throw runtime_error ("unsupported hash algorithm");

    ifstream ifs (p, ios::binary);
    if (!ifs)
      throw runtime_error ("failed to open file for hashing: " + p.string ());

    blake3_hasher hasher;
    blake3_hasher_init (&hasher);

    // Read and hash in sensible chunks.
    //
    char buf[8192];
    while (ifs.read (buf, sizeof (buf)) || ifs.gcount () > 0)
    {
      blake3_hasher_update (&hasher,
                            buf,
                            static_cast<size_t> (ifs.gcount ()));
    }

    if (ifs.bad ())
      throw runtime_error ("error reading file for hashing: " + p.string ());

    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize (&hasher, output, BLAKE3_OUT_LEN);

    ostringstream oss;
    for (size_t i (0); i < BLAKE3_OUT_LEN; ++i)
      oss << hex << setw (2) << setfill ('0') << static_cast<int> (output[i]);

    return oss.str ();
  }

  bool
  compare_hashes (const string& h1, const string& h2)
  {
    if (h1.size () != h2.size ())
      return false;

    // Case-insensitive comparison.
    //
    return equal (h1.begin (),
                  h1.end (),
                  h2.begin (),
                  [] (char a, char b)
    {
      return tolower (static_cast<unsigned char> (a)) ==
             tolower (static_cast<unsigned char> (b));
    });
  }
}
