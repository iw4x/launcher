#include <launcher/steam/steam-manifest.hxx>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <limits>

#include <launcher/launcher-log.hxx>
#include <launcher/steam/steam-compression.hxx>

using namespace std;

namespace launcher
{
  namespace
  {
    constexpr auto trace_l2 ([] (auto&&... a)
    {
      log::trace_l2 (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr uint32_t magic_payload   = 0x71F617D0;
    constexpr uint32_t magic_metadata  = 0x1F4812BE;
    constexpr uint32_t magic_signature = 0x1B81B817;
    constexpr uint32_t magic_end       = 0x32C415AB;

    constexpr uint32_t magic_legacy = 0x16349781;

    enum : uint32_t
    {
      metadata_depot_id            = 1,
      metadata_gid_manifest        = 2,
      metadata_creation_time       = 3,
      metadata_filenames_encrypted = 4,
      metadata_cb_disk_original    = 5,
      metadata_cb_disk_compressed  = 6,
      metadata_unique_chunks       = 7
    };

    enum : uint32_t
    {
      payload_mappings = 1,

      mapping_filename     = 1,
      mapping_size         = 2,
      mapping_flags        = 3,
      mapping_sha_filename = 4,
      mapping_sha_content  = 5,
      mapping_chunks       = 6,
      mapping_linktarget   = 7,

      chunk_sha            = 1,
      chunk_crc            = 2,
      chunk_offset         = 3,
      chunk_cb_original    = 4,
      chunk_cb_compressed  = 5
    };

    uint32_t
    get_u32_le (pb::byte_view b, size_t o) noexcept
    {
      assert (o + 4 <= b.size ());

      return static_cast<uint32_t> (b[o]) |
             static_cast<uint32_t> (b[o + 1]) << 8 |
             static_cast<uint32_t> (b[o + 2]) << 16 |
             static_cast<uint32_t> (b[o + 3]) << 24;
    }

    string
    decrypt_name (const crypto::aes_key& k, string_view encoded)
    {
      crypto::byte_buffer c (crypto::base64_decode (encoded));

      crypto::byte_buffer p (crypto::symmetric_decrypt (
        k, pb::byte_view (c.data (), c.size ())));

      if (!p.empty () && p.back () == 0)
        p.pop_back ();

      return string (p.begin (), p.end ());
    }
  }

  steam_manifest_error::
  steam_manifest_error (const string& w)
    : runtime_error (w)
  {
  }

  string steam_depot_chunk::
  name () const
  {
    return crypto::hex_encode (pb::byte_view (id.data (), id.size ()));
  }

  uint64_t steam_depot_manifest::
  payload_size () const noexcept
  {
    uint64_t r (0);

    for (const steam_depot_file& f : files)
    {
      if (!f.directory () && !f.symlink ())
        r += f.size;
    }

    return r;
  }

  size_t steam_depot_manifest::
  file_count () const noexcept
  {
    return static_cast<size_t> (
      ranges::count_if (files, [] (const steam_depot_file& f)
      {
        return !f.directory () && !f.symlink ();
      }));
  }

  string
  normalize_manifest_path (string_view raw)
  {
    if (raw.empty ())
      throw steam_manifest_error ("the manifest contains an entry with an "
                                  "empty path");

    string s (raw);

    for (char& c : s)
    {
      if (c == '\\')
        c = '/';
    }

    if (s.front () == '/')
      throw steam_manifest_error (
        "the manifest contains an absolute path: '" + string (raw) + "'");

    if (s.size () >= 2 && s[1] == ':' &&
        isalpha (static_cast<unsigned char> (s[0])))
      throw steam_manifest_error (
        "the manifest contains a drive-qualified path: '" + string (raw) +
        "'");

    if (s.find ('\0') != string::npos)
      throw steam_manifest_error (
        "the manifest contains a path with an embedded null byte");

    vector<string_view> parts;

    for (size_t b (0); b <= s.size ();)
    {
      size_t e (s.find ('/', b));

      if (e == string::npos)
        e = s.size ();

      string_view p (s.data () + b, e - b);

      b = e + 1;

      if (p.empty () || p == ".")
        continue;

      if (p == "..")
      {
        if (parts.empty ())
          throw steam_manifest_error (
            "the manifest contains a path that escapes the installation "
            "directory: '" + string (raw) + "'");

        parts.pop_back ();
        continue;
      }

      parts.push_back (p);
    }

    if (parts.empty ())
      throw steam_manifest_error (
        "the manifest contains a path that names no file: '" + string (raw) +
        "'");

    string r;

    for (size_t i (0); i != parts.size (); ++i)
    {
      if (i != 0)
        r += '/';

      r.append (parts[i]);
    }

    return r;
  }

  namespace
  {
    void
    parse_metadata (pb::reader r, steam_depot_manifest& m)
    {
      while (r.next ())
      {
        switch (r.field_number ())
        {
        case metadata_depot_id:
          m.depot_id = r.read_uint32 ();
          break;

        case metadata_gid_manifest:
          m.manifest_id = r.read_uint64 ();
          break;

        case metadata_creation_time:
          m.creation_time = r.read_uint32 ();
          break;

        case metadata_filenames_encrypted:
          m.filenames_encrypted = r.read_bool ();
          break;

        case metadata_cb_disk_original:
          m.total_uncompressed = r.read_uint64 ();
          break;

        case metadata_cb_disk_compressed:
          m.total_compressed = r.read_uint64 ();
          break;

        case metadata_unique_chunks:
          m.unique_chunks = r.read_uint32 ();
          break;

        default:
          r.skip ();
          break;
        }
      }
    }

    steam_depot_chunk
    parse_chunk (pb::reader r)
    {
      steam_depot_chunk c;

      bool have_sha (false);

      while (r.next ())
      {
        switch (r.field_number ())
        {
        case chunk_sha:
          {
            pb::byte_view b (r.read_bytes ());

            if (b.size () != c.id.size ())
              throw steam_manifest_error (
                "the manifest contains a chunk with a " +
                std::to_string (b.size ()) +
                " byte identifier, expected " + std::to_string (c.id.size ()));

            ranges::copy (b, c.id.begin ());
            have_sha = true;

            break;
          }

        case chunk_crc:
          c.checksum = r.read_fixed32 ();
          break;

        case chunk_offset:
          c.offset = r.read_uint64 ();
          break;

        case chunk_cb_original:
          c.uncompressed_size = r.read_uint32 ();
          break;

        case chunk_cb_compressed:
          c.compressed_size = r.read_uint32 ();
          break;

        default:
          r.skip ();
          break;
        }
      }

      if (!have_sha)
        throw steam_manifest_error (
          "the manifest contains a chunk with no identifier");

      return c;
    }

    steam_depot_file
    parse_mapping (pb::reader r)
    {
      steam_depot_file f;

      string raw_name;

      while (r.next ())
      {
        switch (r.field_number ())
        {
        case mapping_filename:
          raw_name = string (r.read_string ());
          break;

        case mapping_size:
          f.size = r.read_uint64 ();
          break;

        case mapping_flags:
          f.flags = r.read_uint32 ();
          break;

        case mapping_sha_filename:
          r.skip ();
          break;

        case mapping_sha_content:
          {
            pb::byte_view b (r.read_bytes ());

            if (b.size () == f.content_sha.size ())
              ranges::copy (b, f.content_sha.begin ());

            break;
          }

        case mapping_chunks:
          f.chunks.push_back (parse_chunk (r.read_message ()));
          break;

        case mapping_linktarget:
          f.link_target = string (r.read_string ());
          break;

        default:
          r.skip ();
          break;
        }
      }

      f.path = move (raw_name);

      return f;
    }
  }

  steam_depot_manifest
  parse_depot_manifest (pb::byte_view d, const crypto::aes_key* k)
  {
    steam_depot_manifest m;

    bool have_payload (false);
    bool have_metadata (false);

    size_t o (0);

    pb::byte_view payload;

    for (;;)
    {
      if (d.size () - o < 4)
        throw steam_manifest_error (
          "the manifest ends without an end-of-manifest marker");

      uint32_t magic (get_u32_le (d, o));
      o += 4;

      if (magic == magic_end)
        break;

      if (magic == magic_legacy)
        throw steam_manifest_error (
          "the manifest uses the pre-protobuf format, which this launcher "
          "does not implement");

      if (magic != magic_payload && magic != magic_metadata &&
          magic != magic_signature)
        throw steam_manifest_error (
          "the manifest contains an unrecognized section marker at offset " +
          std::to_string (o - 4));

      if (d.size () - o < 4)
        throw steam_manifest_error (
          "the manifest section at offset " + std::to_string (o - 4) +
          " has no length");

      uint32_t n (get_u32_le (d, o));
      o += 4;

      if (n > d.size () - o)
        throw steam_manifest_error (
          "the manifest section at offset " + std::to_string (o - 8) +
          " declares " + std::to_string (n) + " bytes but only " +
          std::to_string (d.size () - o) + " remain");

      pb::byte_view s (d.subspan (o, n));
      o += n;

      try
      {
        switch (magic)
        {
        case magic_payload:
          payload      = s;
          have_payload = true;
          break;

        case magic_metadata:
          parse_metadata (pb::reader (s), m);
          have_metadata = true;
          break;

        case magic_signature:

          break;
        }
      }
      catch (const pb::decode_error& e)
      {
        throw steam_manifest_error (string ("malformed manifest section: ") +
                                    e.what ());
      }
    }

    if (!have_payload || !have_metadata)
      throw steam_manifest_error (
        "the manifest is missing its " +
        string (have_payload ? "metadata" : "payload") + " section");

    if (m.filenames_encrypted && k == nullptr)
      trace_l2 ("manifest {} has encrypted filenames and no depot key was "
                "supplied; paths will not be usable",
                m.manifest_id);

    try
    {
      pb::reader r (payload);

      while (r.next ())
      {
        if (r.field_number () != payload_mappings)
        {
          r.skip ();
          continue;
        }

        m.files.push_back (parse_mapping (r.read_message ()));
      }
    }
    catch (const pb::decode_error& e)
    {
      throw steam_manifest_error (string ("malformed manifest payload: ") +
                                  e.what ());
    }

    if (m.filenames_encrypted && k != nullptr)
    {
      for (steam_depot_file& f : m.files)
      {
        try
        {
          f.path = decrypt_name (*k, f.path);

          if (!f.link_target.empty ())
            f.link_target = decrypt_name (*k, f.link_target);
        }
        catch (const crypto::crypto_error& e)
        {
          throw steam_manifest_error (
            string ("failed to decrypt a filename in manifest ") +
            std::to_string (m.manifest_id) +
            " (the depot key may be wrong): " + e.what ());
        }
      }

      m.filenames_encrypted = false;
    }

    if (!m.filenames_encrypted)
    {
      for (steam_depot_file& f : m.files)
      {
        f.path = normalize_manifest_path (f.path);

        for (const steam_depot_chunk& c : f.chunks)
        {
          if (c.uncompressed_size > f.size ||
              c.offset > f.size - c.uncompressed_size)
            throw steam_manifest_error (
              "the manifest describes a chunk at offset " +
              std::to_string (c.offset) + " of size " +
              std::to_string (c.uncompressed_size) + " in '" + f.path +
              "', which is only " + std::to_string (f.size) + " bytes");
        }
      }

      ranges::stable_sort (m.files,
                           [] (const steam_depot_file& a,
                               const steam_depot_file& b)
      {
        return a.path < b.path;
      });
    }

    trace_l2 ("parsed manifest {} of depot {}: {} entries, {} bytes",
              m.manifest_id,
              m.depot_id,
              m.files.size (),
              m.payload_size ());

    return m;
  }

  steam_depot_manifest
  parse_depot_manifest_archive (pb::byte_view a, const crypto::aes_key* k)
  {
    crypto::byte_buffer b;

    try
    {
      b = compression::zip_decompress (a);
    }
    catch (const compression::compression_error& e)
    {
      throw steam_manifest_error (
        string ("failed to unpack the manifest archive: ") + e.what ());
    }

    return parse_depot_manifest (pb::byte_view (b.data (), b.size ()), k);
  }
}
