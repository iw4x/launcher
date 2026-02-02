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

#include <miniz.h>

using namespace std;

namespace launcher
{
  manifest_coordinator::manifest_type manifest_coordinator::
  parse (const string& s, manifest_format f)
  {
    if (s.empty ())
      throw runtime_error ("manifest JSON is empty");

    // Parse the JSON and then immediately link the internal structures. We
    // need the file entries to know about their parent archives (if any)
    // before we hand this object back to the caller.
    //
    manifest_type m (s, f);
    m.link_files ();

    return m;
  }

  manifest_coordinator::manifest_type manifest_coordinator::
  load (const fs::path& f, manifest_format fmt)
  {
    if (!fs::exists (f))
      throw runtime_error ("manifest file does not exist: " + f.string ());

    ifstream is (f);
    if (!is)
      throw runtime_error ("failed to open manifest file: " + f.string ());

    // Slurp the file content into a string.
    //
    string s ((istreambuf_iterator<char> (is)), istreambuf_iterator<char> ());

    return parse (s, fmt);
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

  // Path resolution.
  //

  fs::path manifest_coordinator::
  resolve_path (const file_type& f, const fs::path& d)
  {
    // This is where things get specific to IW4x. We have to map legacy paths
    // (like "codo/") to the actual zone directory and determine where loose
    // files like .iwd or .ff should live if their path isn't explicit.
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
      return d / s;
    }

    // If the path has a known prefix, trust it.
    //
    if (f.path.find ("zone/") == 0 || f.path.find ("zone\\") == 0 ||
        f.path.find ("iw4x/") == 0 || f.path.find ("iw4x\\") == 0)
    {
      return d / f.path;
    }

    // Heuristics for loose files.
    //
    if (ext == ".iwd") return d / "iw4x" / p.filename ();
    if (ext == ".ff")  return d / "zone" / "dlc" / p.filename ();

    return d / f.path;
  }

  fs::path manifest_coordinator::
  resolve_path (const archive_type& a, const fs::path& d)
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
      return d / a.name;
    }

    // Heuristics.
    //
    if (ext == ".iwd") return d / "iw4x" / p.filename ();
    if (ext == ".ff")  return d / "zone" / "dlc" / p.filename ();

    // ZIP archives usually extract in-place at the root.
    //
    if (ext == ".zip") return d / p.filename ();

    return d / a.name;
  }

  // Extraction.
  //

  asio::awaitable<void> manifest_coordinator::
  extract_archive (const archive_type& a,
                   const fs::path& ap,
                   const fs::path& d)
  {
    if (!fs::exists (ap))
      throw runtime_error ("archive file does not exist: " + ap.string ());

    mz_zip_archive z;
    memset (&z, 0, sizeof (z));

    if (!mz_zip_reader_init_file (&z, ap.string ().c_str (), 0))
      throw runtime_error ("failed to open archive: " + ap.string ());

    try
    {
      // If the archive metadata lists specific files, we only extract those.
      // Otherwise, we default to extracting everything.
      //
      if (!a.files.empty ())
      {
        for (const auto& f : a.files)
        {
          int idx (mz_zip_reader_locate_file (&z,
                                              f.path.c_str (),
                                              nullptr,
                                              0));
          if (idx < 0)
            continue;

          fs::path out (resolve_path (f, d));

          if (out.has_parent_path ())
          {
            error_code ec;
            fs::create_directories (out.parent_path (), ec);

            if (ec)
              throw runtime_error ("failed to create directory: " +
                                   out.parent_path ().string ());
          }

          if (!mz_zip_reader_extract_to_file (&z,
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
        mz_uint n (mz_zip_reader_get_num_files (&z));

        for (mz_uint i (0); i < n; ++i)
        {
          mz_zip_archive_file_stat st;
          if (!mz_zip_reader_file_stat (&z, i, &st))
            throw runtime_error ("failed to read file stat from archive");

          if (mz_zip_reader_is_file_a_directory (&z, i))
            continue;

          file_type f;
          f.path = st.m_filename;

          fs::path out (resolve_path (f, d));

          if (out.has_parent_path ())
          {
            error_code ec;
            fs::create_directories (out.parent_path (), ec);

            if (ec)
              throw runtime_error ("failed to create directory: " +
                                   out.parent_path ().string ());
          }

          if (!mz_zip_reader_extract_to_file (&z,
                                              i,
                                              out.string ().c_str (),
                                              0))
          {
            throw runtime_error ("failed to extract file: " +
                                 string (st.m_filename));
          }
        }
      }

      mz_zip_reader_end (&z);
    }
    catch (...)
    {
      // Clean up C resource before throwing.
      //
      mz_zip_reader_end (&z);
      throw;
    }

    co_return;
  }

  // Metrics.
  //

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
}
