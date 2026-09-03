#include <launcher/protobuf/protobuf-wire.hxx>

#include <bit>
#include <cassert>
#include <cstring>
#include <limits>

using namespace std;

namespace launcher
{
  namespace pb
  {
    string
    to_string (wire_type t)
    {
      switch (t)
      {
      case wire_type::varint:           return "varint";
      case wire_type::fixed64:          return "fixed64";
      case wire_type::length_delimited: return "length-delimited";
      case wire_type::start_group:      return "start-group";
      case wire_type::end_group:        return "end-group";
      case wire_type::fixed32:          return "fixed32";
      }

      return "wire-type-" + std::to_string (static_cast<unsigned> (t));
    }

    decode_error::
    decode_error (size_t o, const string& w)
      : runtime_error ("protobuf decode error at offset " +
                       std::to_string (o) + ": " + w),
        offset_ (o)
    {
    }

    static constexpr size_t max_varint_bytes = 10;

    reader::
    reader (byte_view d) noexcept
      : data_ (d),
        position_ (0),
        field_offset_ (0),
        field_ (0),
        type_ (wire_type::varint),
        consumed_ (true),
        started_ (false)
    {
    }

    void reader::
    fail (const string& w) const
    {
      throw decode_error (position_, w);
    }

    bool reader::
    next ()
    {
      if (started_ && !consumed_)
        fail ("field " + std::to_string (field_) +
              " was neither read nor skipped");

      started_ = true;

      if (position_ >= data_.size ())
        return false;

      field_offset_ = position_;

      uint64_t tag (decode_varint ());

      uint64_t fn (tag >> 3);
      uint8_t  wt (static_cast<uint8_t> (tag & 0x07));

      if (fn == 0)
        throw decode_error (field_offset_, "field number zero is reserved");

      if (fn > max_field_number)
        throw decode_error (field_offset_,
                            "field number " + std::to_string (fn) +
                            " exceeds the protocol maximum");

      switch (static_cast<wire_type> (wt))
      {
      case wire_type::varint:
      case wire_type::fixed64:
      case wire_type::length_delimited:
      case wire_type::fixed32:
        break;

      case wire_type::start_group:
      case wire_type::end_group:
        throw decode_error (field_offset_,
                            "group wire type " + std::to_string (wt) +
                            " on field " + std::to_string (fn) +
                            " is not supported");

      default:
        throw decode_error (field_offset_,
                            "reserved wire type " + std::to_string (wt) +
                            " on field " + std::to_string (fn));
      }

      field_    = static_cast<uint32_t> (fn);
      type_     = static_cast<wire_type> (wt);
      consumed_ = false;

      return true;
    }

    void reader::
    consume ()
    {
      if (!started_)
        fail ("read attempted before the first next()");

      if (consumed_)
        fail ("field " + std::to_string (field_) + " read more than once");

      consumed_ = true;
    }

    void reader::
    expect (wire_type t) const
    {
      if (type_ != t)
        throw decode_error (field_offset_,
                            "field " + std::to_string (field_) +
                            " has wire type " + to_string (type_) +
                            " but was read as " + to_string (t));
    }

    uint64_t reader::
    decode_varint ()
    {
      uint64_t r (0);
      size_t   start (position_);

      for (size_t i (0); i != max_varint_bytes; ++i)
      {
        if (position_ >= data_.size ())
          throw decode_error (start, "truncated varint");

        uint8_t b (data_[position_++]);

        if (i == max_varint_bytes - 1 && (b & 0x7f) > 0x01)
          throw decode_error (start, "varint overflows 64 bits");

        r |= static_cast<uint64_t> (b & 0x7f) << (7 * i);

        if ((b & 0x80) == 0)
          return r;
      }

      throw decode_error (start, "varint exceeds ten bytes");
    }

    byte_view reader::
    take (size_t n)
    {
      if (n > data_.size () - position_)
        fail ("length " + std::to_string (n) + " runs past the end of the " +
              "buffer (" + std::to_string (data_.size () - position_) +
              " bytes remain)");

      byte_view r (data_.subspan (position_, n));
      position_ += n;

      return r;
    }

    void reader::
    skip ()
    {
      consume ();

      switch (type_)
      {
      case wire_type::varint:
        decode_varint ();
        break;

      case wire_type::fixed64:
        take (8);
        break;

      case wire_type::fixed32:
        take (4);
        break;

      case wire_type::length_delimited:
        {
          uint64_t n (decode_varint ());

          if (n > numeric_limits<size_t>::max ())
            fail ("length-delimited field is larger than addressable memory");

          take (static_cast<size_t> (n));
          break;
        }

      case wire_type::start_group:
      case wire_type::end_group:

        assert (false && "group wire types are rejected by next()");
        fail ("group wire types are not supported");
      }
    }

    uint64_t reader::
    read_uint64 ()
    {
      expect (wire_type::varint);
      consume ();

      return decode_varint ();
    }

    uint32_t reader::
    read_uint32 ()
    {
      uint64_t v (read_uint64 ());

      if (v > numeric_limits<uint32_t>::max ())
        throw decode_error (field_offset_,
                            "field " + std::to_string (field_) +
                            " value " + std::to_string (v) +
                            " does not fit in uint32");

      return static_cast<uint32_t> (v);
    }

    int64_t reader::
    read_int64 ()
    {
      return static_cast<int64_t> (read_uint64 ());
    }

    int32_t reader::
    read_int32 ()
    {
      int64_t v (static_cast<int64_t> (read_uint64 ()));

      if (v < numeric_limits<int32_t>::min () ||
          v > numeric_limits<int32_t>::max ())
        throw decode_error (field_offset_,
                            "field " + std::to_string (field_) +
                            " value " + std::to_string (v) +
                            " does not fit in int32");

      return static_cast<int32_t> (v);
    }

    int64_t reader::
    read_sint64 ()
    {
      return zigzag_decode (read_uint64 ());
    }

    int32_t reader::
    read_sint32 ()
    {
      int64_t v (zigzag_decode (read_uint64 ()));

      if (v < numeric_limits<int32_t>::min () ||
          v > numeric_limits<int32_t>::max ())
        throw decode_error (field_offset_,
                            "field " + std::to_string (field_) +
                            " zigzag value " + std::to_string (v) +
                            " does not fit in sint32");

      return static_cast<int32_t> (v);
    }

    bool reader::
    read_bool ()
    {
      return read_uint64 () != 0;
    }

    uint64_t reader::
    read_fixed64 ()
    {
      expect (wire_type::fixed64);
      consume ();

      byte_view b (take (8));
      uint64_t  r (0);

      for (size_t i (0); i != 8; ++i)
        r |= static_cast<uint64_t> (b[i]) << (8 * i);

      return r;
    }

    uint32_t reader::
    read_fixed32 ()
    {
      expect (wire_type::fixed32);
      consume ();

      byte_view b (take (4));
      uint32_t  r (0);

      for (size_t i (0); i != 4; ++i)
        r |= static_cast<uint32_t> (b[i]) << (8 * i);

      return r;
    }

    int64_t reader::
    read_sfixed64 ()
    {
      return static_cast<int64_t> (read_fixed64 ());
    }

    int32_t reader::
    read_sfixed32 ()
    {
      return static_cast<int32_t> (read_fixed32 ());
    }

    float reader::
    read_float ()
    {
      static_assert (sizeof (float) == 4, "unsupported float representation");

      return bit_cast<float> (read_fixed32 ());
    }

    double reader::
    read_double ()
    {
      static_assert (sizeof (double) == 8,
                     "unsupported double representation");

      return bit_cast<double> (read_fixed64 ());
    }

    byte_view reader::
    read_bytes ()
    {
      expect (wire_type::length_delimited);
      consume ();

      uint64_t n (decode_varint ());

      if (n > numeric_limits<size_t>::max ())
        fail ("length-delimited field is larger than addressable memory");

      return take (static_cast<size_t> (n));
    }

    string_view reader::
    read_string ()
    {
      byte_view b (read_bytes ());

      return string_view (reinterpret_cast<const char*> (b.data ()),
                          b.size ());
    }

    reader reader::
    read_message ()
    {
      return reader (read_bytes ());
    }

    void reader::
    read_packed_uint64 (vector<uint64_t>& out)
    {
      reader r (read_message ());

      while (r.position_ < r.data_.size ())
        out.push_back (r.decode_varint ());
    }

    void reader::
    read_packed_uint32 (vector<uint32_t>& out)
    {
      reader r (read_message ());

      while (r.position_ < r.data_.size ())
      {
        uint64_t v (r.decode_varint ());

        if (v > numeric_limits<uint32_t>::max ())
          throw decode_error (r.position_,
                              "packed value " + std::to_string (v) +
                              " does not fit in uint32");

        out.push_back (static_cast<uint32_t> (v));
      }
    }

    writer::
    writer (size_t r)
    {
      buffer_.reserve (r);
    }

    size_t
    varint_size (uint64_t v) noexcept
    {
      size_t n (1);

      while (v >= 0x80)
      {
        v >>= 7;
        ++n;
      }

      return n;
    }

    void writer::
    add_varint (uint64_t v)
    {
      while (v >= 0x80)
      {
        buffer_.push_back (static_cast<uint8_t> ((v & 0x7f) | 0x80));
        v >>= 7;
      }

      buffer_.push_back (static_cast<uint8_t> (v));
    }

    void writer::
    add_tag (uint32_t f, wire_type t)
    {
      assert (f != 0 && f <= max_field_number &&
              "field number out of protocol range");

      add_varint ((static_cast<uint64_t> (f) << 3) |
                  static_cast<uint64_t> (t));
    }

    void writer::
    add_le (uint64_t v, size_t w)
    {
      assert ((w == 4 || w == 8) && "unsupported fixed-width size");

      for (size_t i (0); i != w; ++i)
        buffer_.push_back (static_cast<uint8_t> ((v >> (8 * i)) & 0xff));
    }

    void writer::
    add_uint64 (uint32_t f, uint64_t v)
    {
      add_tag (f, wire_type::varint);
      add_varint (v);
    }

    void writer::
    add_uint32 (uint32_t f, uint32_t v)
    {
      add_uint64 (f, v);
    }

    void writer::
    add_int64 (uint32_t f, int64_t v)
    {
      add_tag (f, wire_type::varint);
      add_varint (static_cast<uint64_t> (v));
    }

    void writer::
    add_int32 (uint32_t f, int32_t v)
    {
      add_int64 (f, static_cast<int64_t> (v));
    }

    void writer::
    add_sint64 (uint32_t f, int64_t v)
    {
      add_tag (f, wire_type::varint);
      add_varint (zigzag_encode (v));
    }

    void writer::
    add_sint32 (uint32_t f, int32_t v)
    {
      add_sint64 (f, v);
    }

    void writer::
    add_bool (uint32_t f, bool v)
    {
      add_tag (f, wire_type::varint);
      buffer_.push_back (v ? uint8_t (1) : uint8_t (0));
    }

    void writer::
    add_fixed64 (uint32_t f, uint64_t v)
    {
      add_tag (f, wire_type::fixed64);
      add_le (v, 8);
    }

    void writer::
    add_fixed32 (uint32_t f, uint32_t v)
    {
      add_tag (f, wire_type::fixed32);
      add_le (v, 4);
    }

    void writer::
    add_sfixed64 (uint32_t f, int64_t v)
    {
      add_fixed64 (f, static_cast<uint64_t> (v));
    }

    void writer::
    add_sfixed32 (uint32_t f, int32_t v)
    {
      add_fixed32 (f, static_cast<uint32_t> (v));
    }

    void writer::
    add_float (uint32_t f, float v)
    {
      add_fixed32 (f, bit_cast<uint32_t> (v));
    }

    void writer::
    add_double (uint32_t f, double v)
    {
      add_fixed64 (f, bit_cast<uint64_t> (v));
    }

    void writer::
    add_bytes (uint32_t f, byte_view v)
    {
      add_tag (f, wire_type::length_delimited);
      add_varint (v.size ());

      buffer_.insert (buffer_.end (), v.begin (), v.end ());
    }

    void writer::
    add_string (uint32_t f, string_view v)
    {
      add_bytes (f,
                 byte_view (reinterpret_cast<const uint8_t*> (v.data ()),
                            v.size ()));
    }

    void writer::
    add_message (uint32_t f, const writer& w)
    {
      add_bytes (f, w.view ());
    }

    void writer::
    add_message (uint32_t f, byte_view v)
    {
      add_bytes (f, v);
    }

    void writer::
    add_packed_uint32 (uint32_t f, span<const uint32_t> vs)
    {
      size_t n (0);

      for (uint32_t v : vs)
        n += varint_size (v);

      add_tag (f, wire_type::length_delimited);
      add_varint (n);

      for (uint32_t v : vs)
        add_varint (v);
    }
  }
}
