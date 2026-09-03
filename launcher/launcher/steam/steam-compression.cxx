#include <launcher/steam/steam-compression.hxx>

#include <cassert>
#include <cstdlib>
#include <limits>

#include <miniz.h>

#undef crc32
#undef compress
#undef uncompress

#include <zstd.h>

extern "C"
{
#include <launcher/lzma/LzmaDec.h>
}

using namespace std;

namespace launcher
{
  namespace compression
  {
    compression_error::
    compression_error (const string& w)
      : runtime_error (w)
    {
    }

    namespace
    {
      uint32_t
      get_u32_le (byte_view b, size_t o) noexcept
      {
        assert (o + 4 <= b.size ());

        return static_cast<uint32_t> (b[o]) |
               static_cast<uint32_t> (b[o + 1]) << 8 |
               static_cast<uint32_t> (b[o + 2]) << 16 |
               static_cast<uint32_t> (b[o + 3]) << 24;
      }

      uint16_t
      get_u16_le (byte_view b, size_t o) noexcept
      {
        assert (o + 2 <= b.size ());

        return static_cast<uint16_t> (static_cast<uint16_t> (b[o]) |
                                      static_cast<uint16_t> (b[o + 1]) << 8);
      }

      void
      verify_crc (byte_view d, uint32_t expected, const char* what)
      {
        uint32_t a (static_cast<uint32_t> (
          mz_crc32 (MZ_CRC32_INIT, d.data (), d.size ())));

        if (a != expected)
          throw compression_error (
            string (what) + " checksum mismatch: computed " +
            std::to_string (a) + ", container declares " +
            std::to_string (expected));
      }
    }

    uint32_t
    adler32_steam (byte_view d) noexcept
    {
      constexpr uint32_t base (65521);
      constexpr size_t   run  (5552);

      uint32_t s1 (0);
      uint32_t s2 (0);

      size_t o (0);

      while (o != d.size ())
      {
        size_t n (min (run, d.size () - o));

        for (size_t i (0); i != n; ++i)
        {
          s1 += d[o + i];
          s2 += s1;
        }

        o += n;

        s1 %= base;
        s2 %= base;
      }

      return (s2 << 16) | s1;
    }

    byte_buffer
    gzip_decompress (byte_view d, size_t n)
    {
      constexpr size_t fixed_header (10);
      constexpr size_t trailer (8);

      if (d.size () < fixed_header + trailer)
        throw compression_error ("gzip stream is " +
                                 std::to_string (d.size ()) +
                                 " bytes, too short to contain a header and "
                                 "a trailer");

      if (d[0] != 0x1f || d[1] != 0x8b)
        throw compression_error ("gzip stream does not begin with the gzip "
                                 "magic number");

      if (d[2] != 8)
        throw compression_error ("gzip stream uses compression method " +
                                 std::to_string (d[2]) +
                                 ", but only deflate is supported");

      uint8_t flags (d[3]);
      size_t  o (fixed_header);

      if (flags & 0x04)
      {
        if (d.size () - o < 2)
          throw compression_error ("gzip extra field is truncated");

        size_t n2 (get_u16_le (d, o));
        o += 2;

        if (d.size () - o < n2)
          throw compression_error ("gzip extra field runs past the end of "
                                   "the stream");

        o += n2;
      }

      auto skip_string ([&d, &o] (const char* what)
      {
        for (;; ++o)
        {
          if (o >= d.size ())
            throw compression_error (string ("gzip ") + what +
                                     " field is not terminated");

          if (d[o] == 0)
          {
            ++o;
            break;
          }
        }
      });

      if (flags & 0x08)
        skip_string ("name");

      if (flags & 0x10)
        skip_string ("comment");

      if (flags & 0x02)
      {
        if (d.size () - o < 2)
          throw compression_error ("gzip header checksum is truncated");

        o += 2;
      }

      if (d.size () - o < trailer)
        throw compression_error ("gzip stream is truncated before its "
                                 "trailer");

      size_t dn (d.size () - o - trailer);

      uint32_t tcrc (get_u32_le (d, d.size () - 8));
      uint32_t tsize (get_u32_le (d, d.size () - 4));

      if (tsize != n)
        throw compression_error (
          "gzip trailer declares " + std::to_string (tsize) +
          " bytes but the caller expects " + std::to_string (n));

      byte_buffer r (n);

      if (n != 0)
      {
        mz_ulong rn (static_cast<mz_ulong> (r.size ()));

        mz_stream s {};

        if (mz_inflateInit2 (&s, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
          throw compression_error ("failed to initialize the deflate "
                                   "decoder");

        s.next_in   = d.data () + o;
        s.avail_in  = static_cast<unsigned int> (dn);
        s.next_out  = r.data ();
        s.avail_out = static_cast<unsigned int> (rn);

        int rc (mz_inflate (&s, MZ_FINISH));
        size_t produced (s.total_out);

        mz_inflateEnd (&s);

        if (rc != MZ_STREAM_END)
          throw compression_error (
            string ("failed to inflate the gzip stream: ") +
            mz_error (rc) + " (" + std::to_string (rc) + ")");

        if (produced != n)
          throw compression_error (
            "gzip stream inflated to " + std::to_string (produced) +
            " bytes, expected " + std::to_string (n));
      }
      else if (dn == 0)
        throw compression_error ("gzip stream has no deflate payload");

      verify_crc (byte_view (r.data (), r.size ()), tcrc, "gzip");

      return r;
    }

    namespace
    {
      void*
      lzma_alloc (ISzAllocPtr, size_t n)
      {
        return malloc (n);
      }

      void
      lzma_free (ISzAllocPtr, void* p)
      {
        free (p);
      }

      const ISzAlloc lzma_allocator {lzma_alloc, lzma_free};
    }

    byte_buffer
    vzip_decompress (byte_view d, bool verify)
    {
      constexpr size_t header (7);
      constexpr size_t props (LZMA_PROPS_SIZE);
      constexpr size_t footer (10);

      static_assert (props == 5, "unexpected lzma property size");

      if (d.size () < header + props + footer)
        throw compression_error ("vzip container is " +
                                 std::to_string (d.size ()) +
                                 " bytes, too short to be well-formed");

      if (d[0] != 'V' || d[1] != 'Z')
        throw compression_error ("vzip container does not begin with the "
                                 "expected magic");

      if (d[2] != 'a')
        throw compression_error (string ("vzip container declares version '") +
                                 static_cast<char> (d[2]) +
                                 "', but only 'a' is supported");

      if (d[d.size () - 2] != 'z' || d[d.size () - 1] != 'v')
        throw compression_error ("vzip container does not end with the "
                                 "expected footer magic");

      uint32_t crc (get_u32_le (d, d.size () - footer));
      uint32_t on (get_u32_le (d, d.size () - footer + 4));

      constexpr uint32_t sane_limit (64u * 1024 * 1024);

      if (on > sane_limit)
        throw compression_error (
          "vzip container declares an implausible decompressed size of " +
          std::to_string (on) + " bytes");

      byte_view p (d.subspan (header, props));
      byte_view s (d.subspan (header + props,
                              d.size () - header - props - footer));

      byte_buffer r (on);

      SizeT       dn (on);
      SizeT       sn (s.size ());
      ELzmaStatus st (LZMA_STATUS_NOT_SPECIFIED);

      SRes rc (LzmaDecode (r.data (),
                           &dn,
                           s.data (),
                           &sn,
                           p.data (),
                           static_cast<unsigned> (props),
                           LZMA_FINISH_END,
                           &st,
                           &lzma_allocator));

      if (rc != SZ_OK)
        throw compression_error ("failed to decode the lzma stream (error " +
                                 std::to_string (rc) + ", status " +
                                 std::to_string (st) + ")");

      if (dn != on)
        throw compression_error (
          "lzma stream produced " + std::to_string (dn) +
          " bytes, but the container declares " + std::to_string (on));

      if (verify)
        verify_crc (byte_view (r.data (), r.size ()), crc, "vzip");

      return r;
    }

    byte_buffer
    vzstd_decompress (byte_view d, bool verify)
    {
      constexpr size_t header (8);
      constexpr size_t footer (15);

      if (d.size () < header + footer)
        throw compression_error ("vzstd container is " +
                                 std::to_string (d.size ()) +
                                 " bytes, too short to be well-formed");

      if (d[0] != 'V' || d[1] != 'S' || d[2] != 'Z' || d[3] != 'a')
        throw compression_error ("vzstd container does not begin with the "
                                 "expected magic");

      if (d[d.size () - 3] != 'z' || d[d.size () - 2] != 's' ||
          d[d.size () - 1] != 'v')
        throw compression_error ("vzstd container does not end with the "
                                 "expected footer magic");

      uint32_t crc (get_u32_le (d, d.size () - footer));
      uint32_t on (get_u32_le (d, d.size () - footer + 4));

      constexpr uint32_t sane_limit (64u * 1024 * 1024);

      if (on > sane_limit)
        throw compression_error (
          "vzstd container declares an implausible decompressed size of " +
          std::to_string (on) + " bytes");

      byte_view s (d.subspan (header, d.size () - header - footer));

      byte_buffer r (on);

      size_t dn (ZSTD_decompress (
        r.data (), r.size (), s.data (), s.size ()));

      if (ZSTD_isError (dn))
        throw compression_error (
          string ("failed to decode the zstd frame: ") +
          ZSTD_getErrorName (dn));

      if (dn != on)
        throw compression_error (
          "zstd frame produced " + std::to_string (dn) +
          " bytes, but the container declares " + std::to_string (on));

      if (verify)
        verify_crc (byte_view (r.data (), r.size ()), crc, "vzstd");

      return r;
    }

    byte_buffer
    zip_decompress (byte_view d, bool verify)
    {
      mz_zip_archive z {};

      if (!mz_zip_reader_init_mem (&z, d.data (), d.size (), 0))
        throw compression_error ("failed to open the chunk's zip container");

      struct guard
      {
        mz_zip_archive* z;

        ~guard ()
        {
          mz_zip_reader_end (z);
        }
      } g {&z};

      mz_uint n (mz_zip_reader_get_num_files (&z));

      if (n != 1)
        throw compression_error ("chunk zip container holds " +
                                 std::to_string (n) +
                                 " entries, expected exactly one");

      mz_zip_archive_file_stat st;

      if (!mz_zip_reader_file_stat (&z, 0, &st))
        throw compression_error ("failed to stat the chunk's zip entry");

      if (st.m_uncomp_size > numeric_limits<size_t>::max ())
        throw compression_error ("chunk zip entry is too large to extract");

      byte_buffer r (static_cast<size_t> (st.m_uncomp_size));

      if (!mz_zip_reader_extract_to_mem (&z, 0, r.data (), r.size (), 0))
        throw compression_error ("failed to extract the chunk's zip entry");

      if (verify)
        verify_crc (byte_view (r.data (), r.size ()), st.m_crc32, "zip");

      return r;
    }

    byte_buffer
    decompress_depot_chunk (byte_view d, size_t n)
    {
      if (d.size () < 4)
        throw compression_error ("decrypted chunk is " +
                                 std::to_string (d.size ()) +
                                 " bytes, too short to identify its "
                                 "compression");

      byte_buffer r;

      if (d[0] == 'V' && d[1] == 'S' && d[2] == 'Z' && d[3] == 'a')
      {
        r = vzstd_decompress (d, false);
      }
      else if (d[0] == 'V' && d[1] == 'Z' && d[2] == 'a')
      {
        r = vzip_decompress (d, false);
      }
      else if (d[0] == 'P' && d[1] == 'K' && d[2] == 0x03 && d[3] == 0x04)
      {
        r = zip_decompress (d, false);
      }
      else
      {
        throw compression_error (
          "decrypted chunk has an unrecognized compression container "
          "(leading bytes " +
          std::to_string (d[0]) + " " + std::to_string (d[1]) + " " +
          std::to_string (d[2]) + " " + std::to_string (d[3]) + ")");
      }

      if (r.size () != n)
        throw compression_error (
          "chunk decompressed to " + std::to_string (r.size ()) +
          " bytes, but its manifest entry declares " + std::to_string (n));

      return r;
    }
  }
}
