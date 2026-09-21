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
  namespace
  {
    // Where IW4x reads its fastfiles from.
    //
    // There are two zone trees to keep apart here. The installation
    // owns "zone" and singleplayer still expects to find the files
    // shipped by Steam there. IW4x owns "zone/iw4x/x86" and multiplayer
    // is arranged to look there for the copies we convert.
    //
    // So once a fastfile becomes ours, it stays under the latter. In
    // particular, conversion should never need to move something out of
    // the stock tree or put a converted file back into it.
    //
    //
    const string zone_root ("zone");
    const string converted_root ("zone/iw4x/x86");

    // IW4x has a similar private place for the rest of its resources.
    //
    // The old layout put these directly under "iw4x". Some manifests
    // still use that spelling, so keep the old root around as the name
    // we recognize and translate it to the directory the game now
    // registers as its base game.
    //
    //
    const string basegame_root ("main/iw4x/x86");
    const string legacy_basegame_root ("iw4x");

    // Turn the zone spelling used by a manifest into the spelling the
    // game should actually see.
    //
    // A manifest normally names the source tree, for example
    // "zone/english/iw4x.ff", and what we want in that case is the same
    // suffix under "zone/iw4x/x86". There is one slightly awkward case
    // in that manifests are allowed to name the destination directly.
    // So accept both spellings and make this operation idempotent.
    //
    //
    fs::path
    zone_path (const string& p)
    {
      string s (p);
      replace (s.begin (), s.end (), '\\', '/');

      // Perhaps the manifest has already done the mapping for us. In
      // that case there is nothing left to do. Apart from being useful
      // for manifests that spell destinations, this means passing a
      // mapped path through here for a second time does not grow
      // another "zone/iw4x/x86" prefix.
      //
      if (s.compare (0, converted_root.size (), converted_root) == 0)
        return fs::path (s);

      // At this point the caller has established that this is a path
      // under "zone". Keep everything following that root and hang it
      // under our zone tree. For example, "zone/english/iw4x.ff"
      // becomes "zone/iw4x/x86/english/iw4x.ff".
      //
      // Note that substr() starts at the end of "zone", not after its
      // separator. The suffix begins with '/', which is exactly what
      // joins the two strings.
      //
      return fs::path (converted_root + s.substr (zone_root.size ()));
    }

    // Do the corresponding mapping for files that live in the base game
    // directory.
    //
    // Here the manifest spelling is the old "iw4x/..." layout. The
    // destination spelling is "main/iw4x/x86/...". As with zone_path(),
    // accept an already mapped path since there is no reason for
    // callers to have to remember whether a particular manifest entry
    // has passed through this translation.
    //
    //
    fs::path
    basegame_path (const string& p)
    {
      // Keep the textual comparison independent of which separator
      // spelling the manifest used.
      //
      string s (p);
      replace (s.begin (), s.end (), '\\', '/');

      // An entry can already name the new base game tree. Leave it
      // alone if so. This is particularly handy while old and new
      // manifests can both exist.
      //
      if (s.compare (0, basegame_root.size (), basegame_root) == 0)
        return fs::path (s);

      // What remains is the old "iw4x" spelling. Drop that root and
      // attach the same suffix to the directory registered by the game.
      // So, for example, "iw4x/images/foo.iwi" becomes
      // "main/iw4x/x86/images/foo.iwi".
      //
      return fs::path (
        basegame_root + s.substr (legacy_basegame_root.size ()));
    }
  }

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
      s.replace (0, 5, "zone/");
      return d / zone_path (s);
    }

    // A manifest path under "zone" names the group the file belongs to.
    // The installation's copy of that group stays where Steam put it,
    // so resolve the path through zone_path() before joining it to the
    // game directory.
    //
    if (f.path.find ("zone/") == 0 || f.path.find ("zone\\") == 0)
      return d / zone_path (f.path);

    // The old base game spelling works the same way. Entries under
    // "iw4x" refer to resources that now live below the base game
    // directory registered by IW4x, so let basegame_path() translate
    // that prefix before constructing the final path.
    //
    if (f.path.find ("iw4x/") == 0 || f.path.find ("iw4x\\") == 0)
      return d / basegame_path (f.path);

    // Heuristics for loose files.
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

    // Trust known prefixes, except that a zone directory only names the
    // group: where the game reads that group from is ours to say.
    //
    if (a.name.find ("zone/") == 0 || a.name.find ("zone\\") == 0)
      return d / zone_path (a.name);

    if (a.name.find ("iw4x/") == 0 || a.name.find ("iw4x\\") == 0)
      return d / basegame_path (a.name);

    // Heuristics.
    //
    if (ext == ".iwd") return d / basegame_path (legacy_basegame_root) / p.filename ();
    if (ext == ".ff")  return d / zone_path ("zone/dlc") / p.filename ();

    // ZIP archives usually extract in-place at the root.
    //
    if (ext == ".zip") return d / p.filename ();

    return d / a.name;
  }

  // Extraction.
  //

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

          r.push_back (move (out));
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

          r.push_back (move (out));
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

    co_return r;
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
