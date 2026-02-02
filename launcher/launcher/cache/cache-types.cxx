#include <launcher/cache/cache-types.hxx>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <launcher/blake3.h>

using namespace std;

namespace launcher
{
  // @@: consider relocating this to a more appropriate module.
  //
  string
  compute_blake3 (const fs::path& f)
  {
    ifstream is (f, ios::binary);
    if (!is)
      return string ();

    blake3_hasher h;
    blake3_hasher_init (&h);

    // Read in 64KB chunks. We allocate this on the heap rather than the stack
    // to keep the frame size "reasonable".
    //
    constexpr size_t n (65536);
    vector<char> b (n);

    while (is)
    {
      is.read (b.data (), n);
      if (size_t c = static_cast<size_t> (is.gcount ()))
        blake3_hasher_update (&h, b.data (), c);
    }

    uint8_t d[BLAKE3_OUT_LEN];
    blake3_hasher_finalize (&h, d, BLAKE3_OUT_LEN);

    // Format as a hex string.
    //
    ostringstream os;
    os << hex << setfill ('0');

    for (int i (0); i < BLAKE3_OUT_LEN; ++i)
      os << setw (2) << static_cast<int> (d[i]);

    return os.str ();
  }
}
