#include <launcher/cache/cache-types.hxx>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
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

  namespace setting_keys
  {
    string
    inst_path (const string& s)
    {
      return "install_path." + s;
    }

    string
    steam_prompt (const string& s, const string& d)
    {
      return "steam_prompt." + s + "." + d;
    }
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
    // @@: throw?
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
    // @@: throw?
    //
    if (h.empty ())
      return false;

    // Compute the actual hash and compare. Note that compute_blake3() returning
    // empty means we failed to read the file, which naturally fails the
    // verification. Fall through and test it.
    //
    // @@: throw?
    //
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
}
