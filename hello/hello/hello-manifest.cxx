#include <hello/hello-manifest.hxx>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <iomanip>

#include <boost/json.hpp>
#include <miniz.h>

#include <hello/blake3.h>

using namespace std;

namespace hello
{
  namespace json = boost::json;

  manifest_coordinator::manifest_type manifest_coordinator::
  parse (const string& s, manifest_format k)
  {
    if (s.empty ())
      throw runtime_error ("manifest JSON is empty");

    // Parse the JSON and then immediately link the internal structures. We
    // need the file entries to know about their parent archives (if any)
    // before we hand this object back to the caller.
    //
    manifest_type m (s, k);
    m.link_files ();

    return m;
  }

  manifest_coordinator::manifest_type manifest_coordinator::
  load (const fs::path& f, manifest_format k)
  {
    if (!fs::exists (f))
      throw runtime_error ("manifest file does not exist: " + f.string ());

    ifstream is (f);
    if (!is)
      throw runtime_error ("failed to open manifest file: " + f.string ());

    // Slurp the file content into a string.
    //
    string s ((istreambuf_iterator<char> (is)), istreambuf_iterator<char> ());

    return parse (s, k);
  }

  void manifest_coordinator::
  save (const manifest_type& m, const fs::path& f)
  {
    string s (m.string ());

    if (f.has_parent_path ())
    {
      error_code ec;
      fs::create_directories (f.parent_path (), ec);

      if (ec)
        throw runtime_error ("failed to create manifest directory: " +
                             ec.message ());
    }

    ofstream os (f);
    if (!os)
      throw runtime_error ("failed to create manifest file: " + f.string ());

    os << s;

    if (!os)
      throw runtime_error ("failed to write manifest file: " + f.string ());
  }

  bool manifest_coordinator::
  validate (const manifest_type& m)
  {
    return m.validate ();
  }

  // Identify files that are missing or corrupt locally.
  //
  // We skip files that are part of an archive (e.g., inside a .zip) as
  // those are handled by the archive verification step.
  //
  vector<manifest_coordinator::file_type> manifest_coordinator::
  get_missing_files (const manifest_type& m,
                     const fs::path& dir,
                     bool verify_hashes)
  {
    vector<file_type> r;

    for (const auto& f : m.files)
    {
      if (f.archive_name)
        continue;

      if (!verify_file (f, dir, verify_hashes))
        r.push_back (f);
    }

    return r;
  }

  vector<manifest_coordinator::archive_type> manifest_coordinator::
  get_missing_archives (const manifest_type& m,
                        const fs::path& dir,
                        archive_cache* cache)
  {
    vector<archive_type> r;

    for (const auto& a : m.archives)
    {
      // If we have a cache and the archive has a known hash, we can check if
      // we've already processed this specific version of the archive.
      //
      if (cache != nullptr && !a.hash.empty ())
      {
        auto e (cache->find (a.name, a.hash));

        // If we found a cache entry, we verify that the files listed in that
        // entry actually exist on disk and match the expected state. If they
        // do, we can save ourselves a download.
        //
        if (e && cache->verify_entry (*e, dir))
          continue;
      }

      // If we are not using the cache or the cache check failed, we fall back
      // to verifying the archive file itself.
      //
      // Note that we generally don't verify the hash strictly during this
      // initial check to save time, unless the caller forces it later.
      //
      if (!verify_archive (a, dir, false))
        r.push_back (a);
    }

    return r;
  }

  bool manifest_coordinator::
  verify_file (const file_type& f, const fs::path& dir, bool verify_hash)
  {
    fs::path p (resolve_path (f, dir));
    error_code ec;

    if (!fs::exists (p, ec) || ec)
      return false;

    // Fast check: verify file size.
    //
    if (fs::file_size (p, ec) != f.size || ec)
      return false;

    // Slow check: verify content hash.
    //
    if (verify_hash && !f.hash.empty ())
    {
      string h (compute_file_hash (p, f.hash.algorithm));

      if (!compare_hashes (h, f.hash.value))
        return false;
    }

    return true;
  }

  bool manifest_coordinator::
  verify_archive (const archive_type& a, const fs::path& dir, bool verify_hash)
  {
    fs::path p (resolve_path (a, dir));
    error_code ec;

    if (!fs::exists (p, ec) || ec)
      return false;

    if (fs::file_size (p, ec) != a.size || ec)
      return false;

    if (verify_hash && !a.hash.empty ())
    {
      string h (compute_file_hash (p, a.hash.algorithm));

      if (!compare_hashes (h, a.hash.value))
        return false;
    }

    return true;
  }

  // Path resolution logic.
  //
  // This is where things get a bit specific to IW4x. We have to map legacy
  // paths (like "codo/") to the actual zone directory, and determine where
  // loose files like .iwd or .ff should live if their path isn't explicit.
  //
  fs::path manifest_coordinator::
  resolve_path (const file_type& f, const fs::path& dir)
  {
    fs::path p (f.path);
    string ext (p.extension ().string ());

    // Normalize extension to lowercase.
    //
    transform (ext.begin (), ext.end (), ext.begin (),
               [] (unsigned char c) { return tolower (c); });

    // Handle the "codo/" prefix remapping. This is a legacy artifact from
    // older iterations of the project.
    //
    if (f.path.find ("codo/") == 0 || f.path.find ("codo\\") == 0)
    {
      string s (f.path);
      s.replace (0, 5, f.path[4] == '/' ? "zone/" : "zone\\");
      return dir / s;
    }

    // If the path already has a known prefix, trust it.
    //
    if (f.path.find ("zone/") == 0 || f.path.find ("zone\\") == 0 ||
        f.path.find ("iw4x/") == 0 || f.path.find ("iw4x\\") == 0)
    {
      return dir / f.path;
    }

    // Heuristics for loose files.
    //
    if (ext == ".iwd")
      return dir / "iw4x" / p.filename ();

    if (ext == ".ff")
      return dir / "zone" / "dlc" / p.filename ();

    return dir / f.path;
  }

  fs::path manifest_coordinator::
  resolve_path (const archive_type& a, const fs::path& dir)
  {
    fs::path p (a.name);
    string ext (p.extension ().string ());

    transform (ext.begin (), ext.end (), ext.begin (),
               [] (unsigned char c) { return tolower (c); });

    // Trust known prefixes.
    //
    if (a.name.find ("zone/") == 0 || a.name.find ("zone\\") == 0 ||
        a.name.find ("iw4x/") == 0 || a.name.find ("iw4x\\") == 0)
    {
      return dir / a.name;
    }

    // Heuristics.
    //
    if (ext == ".iwd")
      return dir / "iw4x" / p.filename ();

    if (ext == ".ff")
      return dir / "zone" / "dlc" / p.filename ();

    // ZIP archives are usually extracted in-place at the root.
    //
    if (ext == ".zip")
      return dir / p.filename ();

    return dir / a.name;
  }

  // Archive extraction with optional caching.
  //
  // We use miniz here for a lightweight dependency. Since it's a C library,
  // we have to be careful with resource management (closing the reader) if
  // exceptions are thrown.
  //
  asio::awaitable<void> manifest_coordinator::
  extract_archive (const archive_type& a,
                   const fs::path& archive_path,
                   const fs::path& dir,
                   archive_cache* cache)
  {
    if (!fs::exists (archive_path))
      throw runtime_error ("archive file does not exist: " +
                           archive_path.string ());

    mz_zip_archive zip;
    memset (&zip, 0, sizeof (zip));

    if (!mz_zip_reader_init_file (&zip, archive_path.string ().c_str (), 0))
      throw runtime_error ("failed to open archive: " + archive_path.string ());

    // Prepare the cache entry. We will populate it as we extract files so
    // that next time we can verify the extraction without doing the work again.
    //
    archive_cache_entry cache_entry;

    if (cache != nullptr)
    {
      cache_entry.archive_name = a.name;
      cache_entry.archive_hash = a.hash;
      cache_entry.archive_size = a.size;
    }

    try
    {
      // If the archive metadata lists specific files, we only extract those.
      // Otherwise, we default to extracting everything in the archive.
      //
      if (!a.files.empty ())
      {
        for (const auto& f : a.files)
        {
          int idx (mz_zip_reader_locate_file (&zip,
                                              f.path.c_str (),
                                              nullptr,
                                              0));
          if (idx < 0)
            continue;

          fs::path out (resolve_path (f, dir));

          if (out.has_parent_path ())
          {
            error_code ec;
            fs::create_directories (out.parent_path (), ec);

            if (ec)
              throw runtime_error ("failed to create directory: " +
                                   out.parent_path ().string ());
          }

          if (!mz_zip_reader_extract_to_file (&zip,
                                              idx,
                                              out.string ().c_str (),
                                              0))
          {
            throw runtime_error ("failed to extract file: " + f.path);
          }

          // Record the extraction in the cache.
          //
          if (cache != nullptr)
          {
            basic_manifest_cache_entry<>::extracted_file ef;
            ef.path = fs::relative (out, dir).string ();
            ef.hash = f.hash;
            ef.size = f.size;
            cache_entry.files.push_back (std::move (ef));
          }
        }
      }
      else
      {
        mz_uint n (mz_zip_reader_get_num_files (&zip));

        for (mz_uint i (0); i < n; ++i)
        {
          mz_zip_archive_file_stat stat;
          if (!mz_zip_reader_file_stat (&zip, i, &stat))
            throw runtime_error ("failed to read file stat from archive");

          if (mz_zip_reader_is_file_a_directory (&zip, i))
            continue;

          file_type f;
          f.path = stat.m_filename;

          fs::path out (resolve_path (f, dir));

          if (out.has_parent_path ())
          {
            error_code ec;
            fs::create_directories (out.parent_path (), ec);

            if (ec)
              throw runtime_error ("failed to create directory: " +
                                   out.parent_path ().string ());
          }

          if (!mz_zip_reader_extract_to_file (&zip,
                                              i,
                                              out.string ().c_str (),
                                              0))
          {
            throw runtime_error ("failed to extract file: " +
                                 string (stat.m_filename));
          }

          // Record the extraction in the cache.
          //
          if (cache != nullptr)
          {
            archive_cache_entry::extracted_file ef;
            ef.path = fs::relative (out, dir).string ();
            ef.size = stat.m_uncomp_size;

            // Since we don't have a manifest hash for this file (it was
            // implicit in the archive), we compute it now so we can verify
            // it later without re-hashing the whole archive.
            //
            ef.hash = hash (
              compute_file_hash (out, hash_algorithm::blake3));

            cache_entry.files.push_back (std::move (ef));
          }
        }
      }

      mz_zip_reader_end (&zip);

      // Commit the entry to the cache now that we know everything succeeded.
      //
      if (cache != nullptr && !cache_entry.files.empty ())
        cache->add (std::move (cache_entry));
    }
    catch (...)
    {
      // Clean up the C resource before propagating the exception.
      //
      mz_zip_reader_end (&zip);
      throw;
    }

    co_return;
  }

  // Metrics and helpers.
  //
  uint64_t manifest_coordinator::
  calculate_download_size (const manifest_type& m, const fs::path& dir)
  {
    uint64_t total (0);

    // Sum up missing loose files.
    //
    for (const auto& f : get_missing_files (m, dir, false))
      total += f.size;

    // Sum up missing archives.
    //
    for (const auto& a : get_missing_archives (m, dir))
      total += a.size;

    return total;
  }

  size_t manifest_coordinator::
  get_file_count (const manifest_type& m)
  {
    size_t c (m.files.size ());

    for (const auto& a : m.archives)
      c += a.files.size ();

    return c;
  }

  bool manifest_coordinator::
  is_empty (const manifest_type& m)
  {
    return m.empty ();
  }

  // Hash computation.
  //
  // Currently we only support BLAKE3. If we ever need to support other
  // algorithms, this is where the dispatch logic would go.
  //
  string
  compute_hash (const void* data, size_t size, hash_algorithm a)
  {
    if (a != hash_algorithm::blake3)
      throw runtime_error ("unsupported hash algorithm");

    blake3_hasher hasher;
    blake3_hasher_init (&hasher);
    blake3_hasher_update (&hasher, data, size);

    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize (&hasher, output, BLAKE3_OUT_LEN);

    // Convert the raw bytes to a hex string.
    //
    ostringstream oss;
    for (size_t i (0); i < BLAKE3_OUT_LEN; ++i)
    {
      oss << hex << setw (2) << setfill ('0')
          << static_cast<int> (output[i]);
    }

    return oss.str ();
  }

  string
  compute_file_hash (const fs::path& file_path, hash_algorithm a)
  {
    if (a != hash_algorithm::blake3)
      throw runtime_error ("unsupported hash algorithm");

    ifstream ifs (file_path, ios::binary);
    if (!ifs)
      throw runtime_error ("failed to open file for hashing: " +
                           file_path.string ());

    blake3_hasher hasher;
    blake3_hasher_init (&hasher);

    // Read and hash the file in sensible chunks.
    //
    char buffer[8192];
    while (ifs.read (buffer, sizeof (buffer)) || ifs.gcount () > 0)
    {
      blake3_hasher_update (&hasher,
                            buffer,
                            static_cast<size_t> (ifs.gcount ()));
    }

    if (ifs.bad ())
      throw runtime_error ("error reading file for hashing: " +
                           file_path.string ());

    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize (&hasher, output, BLAKE3_OUT_LEN);

    // Convert to hex.
    //
    ostringstream oss;
    for (size_t i (0); i < BLAKE3_OUT_LEN; ++i)
    {
      oss << hex << setw (2) << setfill ('0')
          << static_cast<int> (output[i]);
    }

    return oss.str ();
  }

  bool
  compare_hashes (const string& h1, const string& h2)
  {
    if (h1.size () != h2.size ())
      return false;

    // Case-insensitive comparison because hex strings can vary.
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
