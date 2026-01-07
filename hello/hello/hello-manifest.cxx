#include <hello/hello-manifest.hxx>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>
#include <stdexcept>

#include <boost/json.hpp>
#include <miniz.h>

#include <hello/manifest/manifest.hxx>

using namespace std;

namespace hello
{
  namespace json = boost::json;

  manifest_coordinator::manifest_type manifest_coordinator::
  parse (const string& s, manifest_format k)
  {
    if (s.empty ())
      throw runtime_error ("manifest JSON is empty");

    // Parse the JSON and link the internal file structures (e.g., mapping
    // file entries to their parent archives if applicable).
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
  get_missing_archives (const manifest_type& m, const fs::path& dir)
  {
    vector<archive_type> r;

    for (const auto& a : m.archives)
    {
      // For archives, we generally don't verify the hash strictly during the
      // initial check to save time, unless forced.
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

  // Archive extraction.
  //
  // We use miniz here for a lightweight dependency. Note that we need to be
  // careful with the C-style API resource management.
  //
  asio::awaitable<void> manifest_coordinator::
  extract_archive (const archive_type& a,
                   const fs::path& archive_path,
                   const fs::path& dir)
  {
    if (!fs::exists (archive_path))
      throw runtime_error ("archive file does not exist: " +
                           archive_path.string ());

    mz_zip_archive zip;
    memset (&zip, 0, sizeof (zip));

    if (!mz_zip_reader_init_file (&zip, archive_path.string ().c_str (), 0))
      throw runtime_error ("failed to open archive: " + archive_path.string ());

    try
    {
      // If the archive metadata lists specific files, we only extract those.
      // Otherwise, we treat it as a full extraction.
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
        }
      }

      mz_zip_reader_end (&zip);
    }
    catch (...)
    {
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

  // Hash computation stubs.
  //
  // Currently we only support the BLAKE3 algorithm, but the implementation
  // hooks are not yet connected.
  //
  string
  compute_hash (const void*, size_t, hash_algorithm a)
  {
    if (a != hash_algorithm::blake3)
      throw runtime_error ("unsupported hash algorithm");

    throw runtime_error ("blake3 hash computation not implemented");
  }

  string
  compute_file_hash (const fs::path&, hash_algorithm a)
  {
    if (a != hash_algorithm::blake3)
      throw runtime_error ("unsupported hash algorithm");

    throw runtime_error ("blake3 hash computation not implemented");
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
