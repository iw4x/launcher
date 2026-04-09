#include <launcher/cache/cache-types.hxx>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <launcher/blake3.h>

using namespace std;

namespace launcher
{
  ostream&
  operator<< (ostream& o, file_state s)
  {
    switch (s)
    {
      case file_state::valid:   return o << "valid";
      case file_state::stale:   return o << "stale";
      case file_state::missing: return o << "missing";
      case file_state::unknown: return o << "unknown";
    }

    return o;
  }

  ostream&
  operator<< (ostream& o, component_type t)
  {
    switch (t)
    {
      case component_type::client:   return o << "client";
      case component_type::rawfiles: return o << "rawfiles";
      case component_type::dlc:      return o << "dlc";
      case component_type::helper:   return o << "helper";
      case component_type::launcher: return o << "launcher";
    }

    return o;
  }

  ostream&
  operator<< (ostream& o, reconcile_action a)
  {
    switch (a)
    {
      case reconcile_action::none:     return o << "none";
      case reconcile_action::download: return o << "download";
      case reconcile_action::verify:   return o << "verify";
      case reconcile_action::remove:   return o << "remove";
    }

    return o;
  }

  std::int64_t
  get_file_mtime (const fs::path& p)
  {
    // Extract the last write time and convert it to a raw count since epoch.
    // Note that we assume the underlying clock's epoch and resolution map
    // directly to our timestamp requirements.
    //
    auto t (fs::last_write_time (p));
    return t.time_since_epoch ().count ();
  }

  std::int64_t
  current_timestamp ()
  {
    // We use the system clock to obtain the current epoch in seconds.
    //
    auto t (chrono::system_clock::now ());
    auto e (t.time_since_epoch ());

    return chrono::duration_cast<chrono::seconds> (e).count ();
  }

  string
  compute_blake3 (const fs::path& p)
  {
    // Bail out if we cannot open the file. Returning an empty string cleanly
    // signals a failure to the caller.
    //
    ifstream i (p, ios::binary);
    if (!i)
      return string ();

    blake3_hasher h;
    blake3_hasher_init (&h);

    // Read and hash the file in 64K chunks.
    //
    constexpr size_t n (65536);
    vector<char> b (n);

    while (i)
    {
      i.read (b.data (), n);

      // We must update the hasher with exactly the number of bytes actually
      // read. Note that gcount() can be smaller than n on the final read or if
      // we happen to encounter an error.
      //
      size_t c (static_cast<size_t> (i.gcount ()));
      if (c != 0)
        blake3_hasher_update (&h, b.data (), c);
    }

    uint8_t d[BLAKE3_OUT_LEN];
    blake3_hasher_finalize (&h, d, BLAKE3_OUT_LEN);

    // Format the hash as a lowercase hex string.
    //
    ostringstream o;
    o << hex << setfill ('0');

    for (int j (0); j < BLAKE3_OUT_LEN; ++j)
      o << setw (2) << static_cast<int> (d[j]);

    return o.str ();
  }

  bool
  verify_blake3 (const fs::path& p, const string& h)
  {
    // Sanity check. An empty expected hash cannot match anything valid.
    //
    if (h.empty ())
      return false;

    // Compute the actual hash and compare. Note that compute_blake3() returning
    // empty means we failed to read the file, which naturally fails the
    // verification. Fall through and test it.
    //
    string a (compute_blake3 (p));
    if (a.empty ())
      return false;

    string e (h);
    e.erase (remove_if (e.begin (),
                        e.end (),
                        [] (unsigned char c)
    {
      return isspace (c);
    }), e.end ());

    if (a.size () != e.size ())
      return false;

    for (size_t i (0); i < a.size (); ++i)
    {
      if (tolower ((unsigned char) a[i]) != tolower ((unsigned char) e[i]))
      {
        return false;
      }
    }

    return true;
  }

  blake3_hash::
  blake3_hash (std::string hex)
  {
    hex.erase (remove_if (hex.begin (),
                          hex.end (),
                          [] (unsigned char c) { return isspace (c); }),
               hex.end ());

    if (hex.empty ())
      return; // Empty hash is explicitly allowed (means "no hash").

    if (hex.size () != 64)
      throw invalid_argument (
        "invalid blake3 hash length (" + to_string (hex.size ()) +
        " chars, expected 64): " + hex);

    // Normalize to lowercase and validate hex characters in one pass.
    //
    for (char& c : hex)
    {
      auto u (static_cast<unsigned char> (c));

      if (!isxdigit (u))
        throw invalid_argument (
          "invalid blake3 hash character: " + std::string (1, c));

      c = static_cast<char> (tolower (u));
    }

    hex_ = move (hex);
  }

  bool blake3_hash::
  verify_file (const fs::path& p) const
  {
    if (hex_.empty ())
      return false;

    blake3_hash actual (of_file (p));

    return !actual.empty () && *this == actual;
  }

  blake3_hash blake3_hash::
  of_file (const fs::path& p)
  {
    // compute_blake3() already returns a lowercase hex string, so we can
    // construct without re-validation by going through the string constructor
    // which will normalize it.
    //
    std::string h (compute_blake3 (p));

    if (h.empty ())
      return blake3_hash ();

    return blake3_hash (move (h));
  }

  ostream&
  operator<< (ostream& o, const blake3_hash& h)
  {
    return o << h.string ();
  }
}
