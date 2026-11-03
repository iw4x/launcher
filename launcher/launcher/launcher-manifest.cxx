// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-or-later

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

#include <launcher/arch/arch-types.hxx>

using namespace std;

namespace launcher
{
  namespace
  {
    // Zone trees of the installation.
    //
    // The stock fastfiles stay in zone/, where Steam installs them and where
    // singleplayer reads them from. The x86 client reads its fastfiles from
    // zone/iw4x/x86/, which is where we install the ones that we ship. The
    // x64 client reads them from zone/iw4x/x64/, which its manifests name
    // directly.
    //
    const string zone_root ("zone");
    const string iw4x_zone_root ("zone/iw4x/");
    const string converted_root ("zone/iw4x/x86");

    // Base game directory of the x86 client. Earlier manifests name it
    // iw4x/, which we map to it.
    //
    const string basegame_root ("main/iw4x/x86");
    const string legacy_basegame_root ("iw4x");

    // Map the manifest path under zone/ to the client's zone tree (e.g.,
    // zone/english/iw4x.ff to zone/iw4x/x86/english/iw4x.ff). A path that is
    // already in the zone tree of a client is returned as is, which makes the
    // mapping idempotent.
    //
    fs::path
    zone_path (const string& p)
    {
      string s (p);
      replace (s.begin (), s.end (), '\\', '/');

      for (architecture a : architectures)
      {
        string r (iw4x_zone_root + string (to_string (a)));

        if (s.compare (0, r.size (), r) == 0)
          return fs::path (s);
      }

      // Note that the suffix starts with the separator after zone.
      //
      return fs::path (converted_root + s.substr (zone_root.size ()));
    }

    // Map the manifest path under iw4x/ to the base game directory (e.g.,
    // iw4x/images/foo.iwi to main/iw4x/x86/images/foo.iwi). As with
    // zone_path(), a path that is already mapped is returned as is.
    //
    fs::path
    basegame_path (const string& p)
    {
      string s (p);
      replace (s.begin (), s.end (), '\\', '/');

      if (s.compare (0, basegame_root.size (), basegame_root) == 0)
        return fs::path (s);

      return fs::path (
        basegame_root + s.substr (legacy_basegame_root.size ()));
    }

    // Return true if the names are the same ignoring the ASCII case, the way
    // Windows compares file names.
    //
    bool
    same_name (const string& l, const string& r)
    {
      if (l.size () != r.size ())
        return false;

      for (size_t i (0); i != l.size (); ++i)
      {
        if (tolower (static_cast<unsigned char> (l[i])) !=
            tolower (static_cast<unsigned char> (r[i])))
          return false;
      }

      return true;
    }

    // If the manifest path names a client executable in the installation
    // root, then return its current name.
    //
    optional<string>
    client_path (const string& p)
    {
      string s (p);
      replace (s.begin (), s.end (), '\\', '/');

      if (s.find ('/') != string::npos)
        return nullopt;

      for (architecture a : architectures)
      {
        if (same_name (s, string (architecture_executable (a))))
          return s;
      }

      // Earlier releases published the IW4x client under its legacy name.
      // The IW4x (mm) client only comes from its own release archive, whose
      // executable we rename when we install it (see sync_client_x64()).
      //
      if (same_name (s, "iw4x (x86).exe"))
        return string (architecture_executable (architecture::x86));

      return nullopt;
    }

    // Restore the executable bits of an extracted file.
    //
    // miniz writes every file with the default permissions, which drops the
    // executable bits that a program (such as linux/companion in the
    // rawfiles) has in the archive. An archive created on a POSIX system
    // records the file mode in the upper half of the external attributes
    // (the zip "made by" host 3), so we take the executable bits from there.
    //
    // Note that this is best effort: some filesystems (FAT, for example)
    // don't support the bits, which is no reason to fail the extraction.
    //
    void
    restore_executable (mz_zip_archive& z, mz_uint i, const fs::path& f)
    {
#ifndef _WIN32
      mz_zip_archive_file_stat st;
      if (!mz_zip_reader_file_stat (&z, i, &st) ||
          (st.m_version_made_by >> 8) != 3)
        return;

      auto m (static_cast<fs::perms> ((st.m_external_attr >> 16) & 0111));

      if (m == fs::perms::none)
        return;

      error_code ec;
      fs::permissions (f, m, fs::perm_options::add, ec);
#else
      (void) z;
      (void) i;
      (void) f;
#endif
    }
  }

  manifest_coordinator::manifest_type manifest_coordinator::
  parse (const string& s, manifest_format f)
  {
    if (s.empty ())
      throw runtime_error ("manifest JSON is empty");

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

  fs::path manifest_coordinator::
  resolve_path (const file_type& f, const fs::path& d)
  {
    fs::path p (f.path);
    string ext (p.extension ().string ());
    transform (ext.begin (), ext.end (), ext.begin (),
               [] (unsigned char c) { return tolower (c); });

    if (optional<string> c = client_path (f.path))
      return d / *c;

    // Earlier manifests name the zone directory codo/.
    //
    if (f.path.find ("codo/") == 0 || f.path.find ("codo\\") == 0)
    {
      string s (f.path);
      s.replace (0, 5, "zone/");
      return d / zone_path (s);
    }

    if (f.path.find ("zone/") == 0 || f.path.find ("zone\\") == 0)
      return d / zone_path (f.path);

    if (f.path.find ("iw4x/") == 0 || f.path.find ("iw4x\\") == 0)
      return d / basegame_path (f.path);

    // A bare file name goes where the game looks for its type.
    //
    if (ext == ".iwd") return d / basegame_path (legacy_basegame_root) / p.filename ();
    if (ext == ".ff")  return d / zone_path ("zone/dlc") / p.filename ();

    return d / f.path;
  }

  fs::path manifest_coordinator::
  resolve_path (const archive_type& a, const fs::path& d)
  {
    fs::path p (a.name);
    string ext (p.extension ().string ());

    transform (ext.begin (), ext.end (), ext.begin (),
               [] (unsigned char c) { return tolower (c); });

    // As for the files (see above).
    //
    if (a.name.find ("zone/") == 0 || a.name.find ("zone\\") == 0)
      return d / zone_path (a.name);

    if (a.name.find ("iw4x/") == 0 || a.name.find ("iw4x\\") == 0)
      return d / basegame_path (a.name);

    if (ext == ".iwd") return d / basegame_path (legacy_basegame_root) / p.filename ();
    if (ext == ".ff")  return d / zone_path ("zone/dlc") / p.filename ();

    // A zip archive goes into the installation root.
    //
    if (ext == ".zip") return d / p.filename ();

    return d / a.name;
  }

  asio::awaitable<vector<fs::path>> manifest_coordinator::
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

    vector<fs::path> r;

    try
    {
      // If the manifest lists the archive's files, then only extract those.
      // Otherwise, extract everything except the excluded files.
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

          restore_executable (z, static_cast<mz_uint> (idx), out);
          r.push_back (move (out));
        }
      }
      else
      {
        mz_uint n (mz_zip_reader_get_num_files (&z));

        // Some release archives keep their contents in a top-level directory
        // named after the archive (e.g., libiw4x-X.Y.Z/ in libiw4x-X.Y.Z.zip),
        // which we strip. We only strip a directory with the archive's stem
        // as its name since the x64 assets keep everything in zone/.
        //
        string pfx (fs::path (a.name).stem ().string () + '/');

        for (mz_uint i (0); i < n && !pfx.empty (); ++i)
        {
          mz_zip_archive_file_stat st;
          if (!mz_zip_reader_file_stat (&z, i, &st))
            throw runtime_error ("failed to read file stat from archive");

          string s (st.m_filename);
          replace (s.begin (), s.end (), '\\', '/');

          if (s.compare (0, pfx.size (), pfx) != 0 && s + '/' != pfx)
            pfx.clear ();
        }

        for (mz_uint i (0); i < n; ++i)
        {
          mz_zip_archive_file_stat st;
          if (!mz_zip_reader_file_stat (&z, i, &st))
            throw runtime_error ("failed to read file stat from archive");

          if (mz_zip_reader_is_file_a_directory (&z, i))
            continue;

          file_type f;
          f.path = st.m_filename;
          replace (f.path.begin (), f.path.end (), '\\', '/');

          if (!pfx.empty ())
            f.path.erase (0, pfx.size ());

          if (find (a.exclude.begin (), a.exclude.end (), f.path) !=
              a.exclude.end ())
            continue;

          for (const auto& [from, to] : a.rename)
          {
            if (f.path == from)
            {
              f.path = to;
              break;
            }
          }

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

          restore_executable (z, i, out);
          r.push_back (move (out));
        }
      }

      mz_zip_reader_end (&z);
    }
    catch (...)
    {
      mz_zip_reader_end (&z);
      throw;
    }

    co_return r;
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
}
