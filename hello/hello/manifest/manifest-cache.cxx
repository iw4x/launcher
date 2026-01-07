#include <hello/manifest/manifest-cache.hxx>
#include <hello/manifest/manifest.hxx>

#include <fstream>
#include <sstream>
#include <chrono>
#include <stdexcept>

#include <boost/json.hpp>

using namespace std;

namespace hello
{
  // Load the cache from disk.
  //
  // We try to be resilient here: if the file doesn't exist, is empty, or
  // contains invalid JSON, we just treat it as an empty cache and move on.
  //
  template <typename S, typename H>
  void basic_manifest_cache<S, H>::
  load ()
  {
    // Start by clearing the in-memory state so we don't merge with stale
    // data if we fail halfway through.
    //
    entries_.clear ();
    dirty_ = false;

    // If the cache file doesn't exist, there is nothing to load.
    //
    if (!fs::exists (cache_file_))
      return;

    ifstream ifs (cache_file_);
    if (!ifs)
      return;

    stringstream ss;
    ss << ifs.rdbuf ();
    string s (ss.str ());

    if (s.empty ())
      return;

    try
    {
      boost::json::value jv (boost::json::parse (s));

      if (!jv.is_object ())
        return;

      const auto& obj (jv.as_object ());

      // Check for the top-level 'entries' array. If it's missing or valid,
      // we consider the cache empty/corrupt.
      //
      if (!obj.contains ("entries") || !obj.at ("entries").is_array ())
        return;

      // Iterate over the entries and reconstruct our internal representation.
      //
      for (const auto& je : obj.at ("entries").as_array ())
      {
        if (!je.is_object ())
          continue;

        const auto& eo (je.as_object ());
        entry_type e;

        // Archive metadata.
        //
        if (eo.contains ("archive_name") && eo.at ("archive_name").is_string ())
          e.archive_name = boost::json::value_to<string_type> (
            eo.at ("archive_name"));

        if (eo.contains ("archive_hash") && eo.at ("archive_hash").is_string ())
          e.archive_hash = hash_type (
            boost::json::value_to<string_type> (eo.at ("archive_hash")));

        if (eo.contains ("archive_size"))
          e.archive_size = boost::json::value_to<uint64_t> (
            eo.at ("archive_size"));

        if (eo.contains ("timestamp"))
          e.timestamp = boost::json::value_to<uint64_t> (
            eo.at ("timestamp"));

        // Extracted files list.
        //
        // We need to reconstruct the list of files that were extracted from
        // this archive so we can verify them later.
        //
        if (eo.contains ("files") && eo.at ("files").is_array ())
        {
          for (const auto& jf : eo.at ("files").as_array ())
          {
            if (!jf.is_object ())
              continue;

            const auto& fo (jf.as_object ());
            typename entry_type::extracted_file f;

            if (fo.contains ("path") && fo.at ("path").is_string ())
              f.path = boost::json::value_to<string_type> (fo.at ("path"));

            if (fo.contains ("hash") && fo.at ("hash").is_string ())
              f.hash = hash_type (
                boost::json::value_to<string_type> (fo.at ("hash")));

            if (fo.contains ("size"))
              f.size = boost::json::value_to<uint64_t> (fo.at ("size"));

            if (!f.empty ())
              e.files.push_back (move (f));
          }
        }

        if (!e.empty ())
          entries_.push_back (move (e));
      }
    }
    catch (const exception&)
    {
      // If parsing fails, we assume corruption and start fresh. It's better
      // to lose the cache than to operate on bad data.
      //
      entries_.clear ();
    }
  }

  // Save the cache to disk.
  //
  // We serialize the internal state to a JSON object and write it out. Note
  // that we overwrite the existing file completely rather than trying to
  // update it in place.
  //
  template <typename S, typename H>
  void basic_manifest_cache<S, H>::
  save () const
  {
    boost::json::object root;
    boost::json::array entries_arr;

    // Convert our internal entries back to JSON objects.
    //
    for (const auto& e : entries_)
    {
      boost::json::object eo;

      eo["archive_name"] = e.archive_name;
      eo["archive_hash"] = e.archive_hash.value;
      eo["archive_size"] = e.archive_size;
      eo["timestamp"]    = e.timestamp;

      boost::json::array files_arr;

      for (const auto& f : e.files)
      {
        boost::json::object fo;
        fo["path"] = f.path;
        fo["hash"] = f.hash.value;
        fo["size"] = f.size;

        files_arr.push_back (move (fo));
      }

      eo["files"] = move (files_arr);
      entries_arr.push_back (move (eo));
    }

    root["entries"] = move (entries_arr);

    // Write to file.
    //
    ofstream ofs (cache_file_);
    if (!ofs)
      throw runtime_error ("failed to open cache file for writing: " +
                                cache_file_.string ());

    ofs << boost::json::serialize (root);
  }

  // Find a cache entry for a specific archive.
  //
  // We match based on both name and hash to ensure we don't return an entry
  // for a different version of the same archive (e.g., if the archive was
  // updated on the server but kept the same name).
  //
  template <typename S, typename H>
  optional<typename basic_manifest_cache<S, H>::entry_type>
  basic_manifest_cache<S, H>::
  find (const string_type& name, const hash_type& hash) const
  {
    for (const auto& e : entries_)
    {
      if (e.archive_name == name && e.archive_hash.value == hash.value)
        return e;
    }

    return nullopt;
  }

  // Add or update an archive entry.
  //
  // If the entry already exists (matched by name), we remove it first to
  // ensure we don't end up with duplicates. We also ensure the entry has a
  // timestamp if one wasn't provided.
  //
  template <typename S, typename H>
  void basic_manifest_cache<S, H>::
  add (entry_type e)
  {
    remove (e.archive_name);

    if (e.timestamp == 0)
    {
      e.timestamp = static_cast<uint64_t> (
        chrono::system_clock::now ().time_since_epoch ().count ());
    }

    entries_.push_back (move (e));
    dirty_ = true;
  }

  // Remove an entry by archive name.
  //
  template <typename S, typename H>
  void basic_manifest_cache<S, H>::
  remove (const string_type& name)
  {
    auto it (remove_if (entries_.begin (),
                             entries_.end (),
                             [&name] (const entry_type& e)
                             {
                               return e.archive_name == name;
                             }));

    if (it != entries_.end ())
    {
      entries_.erase (it, entries_.end ());
      dirty_ = true;
    }
  }

  // Verify that the cached extraction is still valid on disk.
  //
  // We iterate over every file that was supposedly extracted from this
  // archive and check if it still exists, has the right size, and (optionally)
  // has the right hash. If any file is missing or altered, the entry is
  // considered invalid.
  //
  template <typename S, typename H>
  bool basic_manifest_cache<S, H>::
  verify_entry (const entry_type& e, const fs::path& dir) const
  {
    // We expect these helpers to be available (e.g., from hello-manifest.cxx).
    //
    extern string_type compute_file_hash (const fs::path&, hash_algorithm);
    extern bool compare_hashes (const string_type&, const string_type&);

    for (const auto& f : e.files)
    {
      fs::path p (dir / f.path);
      error_code ec;

      // Existence check.
      //
      if (!fs::exists (p, ec) || ec)
        return false;

      // Size check.
      //
      // Try to catches most partial writes or truncated files.
      //
      if (fs::file_size (p, ec) != f.size || ec)
        return false;

      // Hash check.
      //
      // This is expensive, but if the file has a hash in the cache, we
      // should probably verify it.
      //
      if (!f.hash.empty ())
      {
        string_type h (compute_file_hash (p, f.hash.algorithm));

        if (!compare_hashes (h, f.hash.value))
          return false;
      }
    }

    return true;
  }

  template <typename S, typename H>
  void basic_manifest_cache<S, H>::
  clear ()
  {
    entries_.clear ();
    dirty_ = true;
  }

  // Explicit template instantiation.
  //
  template class basic_manifest_cache<string, hello::basic_hash<string>>;
}
