#include <launcher/launcher-manifest.hxx>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <boost/json.hpp>
#include <launcher/blake3.h>
#include <miniz.h>

using namespace std;

namespace launcher
{
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

  // Verification & Diffing.
  //

  vector<manifest_coordinator::file_type> manifest_coordinator::
  get_missing_files (const manifest_type& m,
                     const fs::path& dir,
                     bool verify_hashes)
  {
    // We skip files that are part of an archive (e.g., inside a .zip) as
    // those are handled by the archive verification step.
    //
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
      // initial check to save time (hashing big zips is slow), unless the
      // caller forces it later.
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

    // Check existence and size first.
    //
    if (!fs::exists (p, ec) || ec)
      return false;

    if (fs::file_size (p, ec) != f.size || ec)
      return false;

    // Only hash if explicitly requested.
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

  // Path resolution.
  //

  fs::path manifest_coordinator::
  resolve_path (const file_type& f, const fs::path& dir)
  {
    // This is where things get specific to IW4x. We have to map legacy
    // paths (like "codo/") to the actual zone directory and determine where
    // loose files like .iwd or .ff should live if their path isn't explicit.
    //
    fs::path p (f.path);
    string ext (p.extension ().string ());
    transform (ext.begin (), ext.end (), ext.begin (),
               [] (unsigned char c) { return tolower (c); });

    // Handle "codo/" remapping. This is a legacy artifact.
    //
    if (f.path.find ("codo/") == 0 || f.path.find ("codo\\") == 0)
    {
      string s (f.path);
      s.replace (0, 5, f.path[4] == '/' ? "zone/" : "zone\\");
      return dir / s;
    }

    // If the path has a known prefix, trust it.
    //
    if (f.path.find ("zone/") == 0 || f.path.find ("zone\\") == 0 ||
        f.path.find ("iw4x/") == 0 || f.path.find ("iw4x\\") == 0)
    {
      return dir / f.path;
    }

    // Heuristics for loose files.
    //
    if (ext == ".iwd") return dir / "iw4x" / p.filename ();
    if (ext == ".ff")  return dir / "zone" / "dlc" / p.filename ();

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
    if (ext == ".iwd") return dir / "iw4x" / p.filename ();
    if (ext == ".ff")  return dir / "zone" / "dlc" / p.filename ();

    // ZIP archives usually extract in-place at the root.
    //
    if (ext == ".zip") return dir / p.filename ();

    return dir / a.name;
  }

  // Extraction.
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

    // Prepare the cache entry. We will populate it as we extract files so that
    // next time we can verify the extraction without doing the work again.
    //
    archive_cache_entry ce;

    if (cache != nullptr)
    {
      ce.archive_name = a.name;
      ce.archive_hash = a.hash;
      ce.archive_size = a.size;
    }

    try
    {
      // If the archive metadata lists specific files, we only extract those.
      // Otherwise, we default to extracting everything.
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

          if (cache != nullptr)
          {
            basic_manifest_cache_entry<>::extracted_file ef;
            ef.path = fs::relative (out, dir).string ();
            ef.hash = f.hash;
            ef.size = f.size;
            ce.files.push_back (move (ef));
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

          if (cache != nullptr)
          {
            archive_cache_entry::extracted_file ef;
            ef.path = fs::relative (out, dir).string ();
            ef.size = stat.m_uncomp_size;

            // We don't have a manifest hash for this file since it was
            // implicit, so compute it now to verify later.
            //
            ef.hash = hash (
              compute_file_hash (out, hash_algorithm::blake3));

            ce.files.push_back (move (ef));
          }
        }
      }

      mz_zip_reader_end (&zip);

      // Commit the entry to the cache.
      //
      if (cache != nullptr && !ce.files.empty ())
        cache->add (move (ce));
    }
    catch (...)
    {
      // Clean up C resource before throwing.
      //
      mz_zip_reader_end (&zip);
      throw;
    }

    co_return;
  }

  // @@: We duplicate most logic from above so we may as well make it generic
  // at some point.
  //
  asio::awaitable<void> manifest_coordinator::
  extract_dxvk_archive (const fs::path& ar, const fs::path& dst)
  {
    mz_zip_archive z;
    memset (&z, 0, sizeof (z));

    if (!mz_zip_reader_init_file (&z, ar.string ().c_str (), 0))
      throw runtime_error ("failed to open DXVK archive: " + ar.string ());

    // We have to ensure we close the reader handle even if we throw
    // halfway through the extraction. Since we don't have an RAII wrapper
    // for miniz, we have to do the try-catch dance.
    //
    try
    {
      mz_uint n (mz_zip_reader_get_num_files (&z));
      bool found (false);

      for (mz_uint i (0); i < n; ++i)
      {
        mz_zip_archive_file_stat s;
        if (!mz_zip_reader_file_stat (&z, i, &s))
          continue;

        string name (s.m_filename);

        // The archive structure might be prefixed with a version directory or
        // it might not. To be safe, we just look for the suffix in the path.
        //
        if (name.size () >= 12 &&
            name.compare (name.size () - 12, 12, "x32/d3d9.dll") == 0)
        {
          fs::path out (dst / "d3d9.dll");

          if (!mz_zip_reader_extract_to_file (&z,
                                              i,
                                              out.string ().c_str (),
                                              0))
            throw runtime_error (
              "failed to extract d3d9.dll from DXVK archive");

          found = true;
          break;
        }
      }

      mz_zip_reader_end (&z);

      if (!found)
        throw runtime_error ("d3d9.dll not found in DXVK archive");
    }
    catch (...)
    {
      mz_zip_reader_end (&z);
      throw;
    }

    co_return;
  }

  // Metrics.
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

  // Hashing.
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

    ostringstream oss;
    for (size_t i (0); i < BLAKE3_OUT_LEN; ++i)
      oss << hex << setw (2) << setfill ('0') << static_cast<int> (output[i]);

    return oss.str ();
  }
}
