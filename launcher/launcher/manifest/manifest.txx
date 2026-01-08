#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <stdexcept>
#include <algorithm>

namespace launcher
{
  template <typename S>
  template <typename Buffer>
  bool basic_hash<S>::
  verify (const Buffer& data) const
  {
    // @@: Currently stubbed.
    //
    return !value.empty ();
  }

  template <typename F, typename T>
  basic_manifest<F, T>::
  basic_manifest (const string_type& json_str, manifest_format k)
    : kind (k)
  {
    try
    {
      // Parse the JSON string.
      //
      // We explicitly check that the root element is an object before proceeding,
      // as a manifest must define properties like 'files' or 'archives'.
      //
      json::value jv (json::parse (json_str));

      if (!jv.is_object ())
        throw std::invalid_argument ("manifest JSON must be an object");

      const json::object& obj (jv.as_object ());

      switch (kind)
      {
        case manifest_format::update:
          parse_update (obj);
          break;
        case manifest_format::dlc:
          parse_dlc (obj);
          break;
      }
    }
    catch (const std::exception& e)
    {
      throw std::runtime_error (string_type ("failed to parse manifest: ") +
                                e.what ());
    }
  }

  template <typename F, typename T>
  basic_manifest<F, T>::
  basic_manifest (const json::value& jv, manifest_format k)
    : kind (k)
  {
    try
    {
      if (!jv.is_object ())
        throw std::invalid_argument ("manifest JSON must be an object");

      const json::object& obj (jv.as_object ());

      switch (kind)
      {
      case manifest_format::update:
        parse_update (obj);
        break;
      case manifest_format::dlc:
        parse_dlc (obj);
        break;
      }
    }
    catch (const std::exception& e)
    {
      throw std::runtime_error (
        string_type ("failed to parse manifest: ") + e.what ());
    }
  }

  template <typename F, typename T>
  typename basic_manifest<F, T>::string_type basic_manifest<F, T>::
  string () const
  {
    // Serialize the internal state back to a JSON string.
    //
    return json::serialize (json ());
  }

  template <typename F, typename T>
  json::value basic_manifest<F, T>::
  json () const
  {
    switch (kind)
    {
      case manifest_format::update:
        return serialize_update ();
      case manifest_format::dlc:
        return serialize_dlc ();
    }

    return json::object ();
  }

  template <typename F, typename T>
  void basic_manifest<F, T>::
  parse_update (const json::object& obj)
  {
    // Parse archives.
    //
    // The 'archives' array contains the raw .iwd or .ff files that need to be
    // downloaded.
    //
    if (obj.contains ("archives") && obj.at ("archives").is_array ())
    {
      for (const auto& ja : obj.at ("archives").as_array ())
      {
        if (!ja.is_object ())
          continue;

        const auto& ao (ja.as_object ());

        archive_type archive;

        if (ao.contains ("blake3") && ao.at ("blake3").is_string ())
          archive.hash = hash_type (
            json::value_to<string_type> (ao.at ("blake3")));

        if (ao.contains ("size"))
        {
          if (ao.at ("size").is_int64 ())
            archive.size = ao.at ("size").as_int64 ();
          else if (ao.at ("size").is_uint64 ())
            archive.size = ao.at ("size").as_uint64 ();
        }

        if (ao.contains ("name") && ao.at ("name").is_string ())
          archive.name = json::value_to<string_type> (ao.at ("name"));

        if (ao.contains ("url") && ao.at ("url").is_string ())
          archive.url = json::value_to<string_type> (ao.at ("url"));

        if (!archive.empty ())
          archives.push_back (std::move (archive));
      }
    }

    // Parse individual files.
    //
    // These entries usually map specific assets to the archives that contain
    // them, or stand-alone files.
    //
    if (obj.contains ("files") && obj.at ("files").is_array ())
    {
      for (const auto& jf : obj.at ("files").as_array ())
      {
        if (!jf.is_object ())
          continue;

        const auto& fo (jf.as_object ());

        file_type file;

        if (fo.contains ("blake3") && fo.at ("blake3").is_string ())
          file.hash = hash_type (
            json::value_to<string_type> (fo.at ("blake3")));

        if (fo.contains ("size"))
        {
          if (fo.at ("size").is_int64 ())
            file.size = fo.at ("size").as_int64 ();
          else if (fo.at ("size").is_uint64 ())
            file.size = fo.at ("size").as_uint64 ();
        }

        if (fo.contains ("path") && fo.at ("path").is_string ())
          file.path = json::value_to<string_type> (fo.at ("path"));

        if (fo.contains ("asset_name") && fo.at ("asset_name").is_string ())
          file.asset_name = json::value_to<string_type> (fo.at ("asset_name"));

        if (fo.contains ("archive") && fo.at ("archive").is_string ())
          file.archive_name = json::value_to<string_type> (fo.at ("archive"));

        if (!file.empty ())
          files.push_back (std::move (file));
      }
    }
  }

  template <typename F, typename T>
  void basic_manifest<F, T>::
  parse_dlc (const json::object& obj)
  {
    // Parse files.
    //
    // DLC manifests typically only contain a list of files (maps, weapons, etc.)
    // without the strict archive structure of the base game update.
    //
    if (obj.contains ("files") && obj.at ("files").is_array ())
    {
      for (const auto& jf : obj.at ("files").as_array ())
      {
        if (!jf.is_object ())
          continue;

        const auto& fo (jf.as_object ());

        file_type file;

        if (fo.contains ("blake3") && fo.at ("blake3").is_string ())
          file.hash = hash_type (
            json::value_to<string_type> (fo.at ("blake3")));

        if (fo.contains ("size"))
        {
          if (fo.at ("size").is_int64 ())
            file.size = fo.at ("size").as_int64 ();
          else if (fo.at ("size").is_uint64 ())
            file.size = fo.at ("size").as_uint64 ();
        }

        if (fo.contains ("path") && fo.at ("path").is_string ())
          file.path = json::value_to<string_type> (fo.at ("path"));

        if (fo.contains ("asset_name") && fo.at ("asset_name").is_string ())
          file.asset_name = json::value_to<string_type> (fo.at ("asset_name"));

        if (!file.empty ())
          files.push_back (std::move (file));
      }
    }
  }

  template <typename F, typename T>
  json::object basic_manifest<F, T>::
  serialize_update () const
  {
    json::object obj;

    // Serialize archives.
    //
    if (!archives.empty ())
    {
      json::array archives_arr;

      for (const auto& archive : archives)
      {
        json::object ao;

        if (!archive.hash.empty ())
          ao["blake3"] = archive.hash.value;

        ao["size"] = archive.size;

        if (!archive.name.empty ())
          ao["name"] = archive.name;

        if (!archive.url.empty ())
          ao["url"] = archive.url;

        archives_arr.push_back (std::move (ao));
      }

      obj["archives"] = std::move (archives_arr);
    }

    // Serialize files.
    //
    if (!files.empty ())
    {
      json::array files_arr;

      for (const auto& file : files)
      {
        json::object fo;

        if (!file.hash.empty ())
          fo["blake3"] = file.hash.value;

        fo["size"] = file.size;

        if (!file.path.empty ())
          fo["path"] = file.path;

        if (file.asset_name)
          fo["asset_name"] = *file.asset_name;

        if (file.archive_name)
          fo["archive"] = *file.archive_name;

        files_arr.push_back (std::move (fo));
      }

      obj["files"] = std::move (files_arr);
    }

    return obj;
  }

  template <typename F, typename T>
  json::object basic_manifest<F, T>::
  serialize_dlc () const
  {
    json::object obj;

    // Serialize files.
    //
    if (!files.empty ())
    {
      json::array files_arr;

      for (const auto& file : files)
      {
        json::object fo;

        if (!file.hash.empty ())
          fo["blake3"] = file.hash.value;

        fo["size"] = file.size;

        if (!file.path.empty ())
          fo["path"] = file.path;

        if (file.asset_name)
          fo["asset_name"] = *file.asset_name;

        files_arr.push_back (std::move (fo));
      }

      obj["files"] = std::move (files_arr);
    }

    return obj;
  }

  template <typename F, typename T>
  asio::awaitable<basic_manifest<F, T>> basic_manifest<F, T>::
  parse_async (const string_type& json_str, manifest_format k)
  {
    // Offload parsing to the coroutine context.
    //
    // While Boost.JSON parsing is synchronous, wrapping it here allows callers
    // to treat it uniformly with other async operations, potentially moving it
    // to a thread pool if parsing large manifests becomes a bottleneck.
    //
    co_return basic_manifest (json_str, k);
  }

  template <typename F, typename T>
  asio::awaitable<bool> basic_manifest<F, T>::
  validate_async () const
  {
    // @@: Currently stubbed.
    //
    co_return validate ();
  }
}
