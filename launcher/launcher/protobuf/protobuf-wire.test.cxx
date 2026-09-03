#include <launcher/protobuf/protobuf-wire.hxx>

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace std;
using namespace launcher;

namespace
{
  using pb::byte_buffer;
  using pb::byte_view;

  byte_view
  view (const byte_buffer& b)
  {
    return byte_view (b.data (), b.size ());
  }

  template <typename F>
  void
  check_rejects (const byte_buffer& b, F f)
  {
    bool threw (false);

    try
    {
      pb::reader r (view (b));
      f (r);
    }
    catch (const pb::decode_error&)
    {
      threw = true;
    }

    assert (threw);
  }

  void
  test_varint_roundtrip ()
  {
    const uint64_t vs[] = {0,
                           1,
                           127,
                           128,
                           300,
                           16383,
                           16384,
                           numeric_limits<uint32_t>::max (),
                           numeric_limits<uint64_t>::max ()};

    for (uint64_t v : vs)
    {
      pb::writer w;
      w.add_uint64 (1, v);

      assert (w.size () == 1 + pb::varint_size (v));

      pb::reader r (w.view ());

      assert (r.next ());
      assert (r.field_number () == 1);
      assert (r.type () == pb::wire_type::varint);
      assert (r.read_uint64 () == v);
      assert (!r.next ());
    }
  }

  void
  test_scalar_roundtrip ()
  {
    pb::writer w;

    w.add_uint32 (1, 4242u);
    w.add_int32 (2, -7);
    w.add_int64 (3, -1);
    w.add_sint32 (4, -12345);
    w.add_sint64 (5, numeric_limits<int64_t>::min ());
    w.add_bool (6, true);
    w.add_fixed64 (7, 0x0102030405060708ull);
    w.add_fixed32 (8, 0xdeadbeefu);
    w.add_float (9, 0.5f);
    w.add_double (10, -0.25);
    w.add_string (11, "hello");

    const uint8_t raw[] = {0xff, 0x00, 0x7f};
    w.add_bytes (12, byte_view (raw, sizeof (raw)));

    pb::reader r (w.view ());

    assert (r.next () && r.field_number () == 1 && r.read_uint32 () == 4242u);
    assert (r.next () && r.field_number () == 2 && r.read_int32 () == -7);
    assert (r.next () && r.field_number () == 3 && r.read_int64 () == -1);
    assert (r.next () && r.field_number () == 4 && r.read_sint32 () == -12345);
    assert (r.next () && r.field_number () == 5 &&
            r.read_sint64 () == numeric_limits<int64_t>::min ());
    assert (r.next () && r.field_number () == 6 && r.read_bool ());
    assert (r.next () && r.field_number () == 7 &&
            r.read_fixed64 () == 0x0102030405060708ull);
    assert (r.next () && r.field_number () == 8 &&
            r.read_fixed32 () == 0xdeadbeefu);
    assert (r.next () && r.field_number () == 9 && r.read_float () == 0.5f);
    assert (r.next () && r.field_number () == 10 && r.read_double () == -0.25);
    assert (r.next () && r.field_number () == 11 &&
            r.read_string () == "hello");

    assert (r.next () && r.field_number () == 12);
    byte_view b (r.read_bytes ());
    assert (b.size () == 3 && b[0] == 0xff && b[1] == 0x00 && b[2] == 0x7f);

    assert (!r.next ());
    assert (r.exhausted ());
  }

  void
  test_negative_int32_sign_extension ()
  {
    pb::writer w;
    w.add_int32 (13, -1);

    assert (w.size () == 1 + 10);

    const byte_buffer& d (w.data ());

    for (size_t i (1); i != 10; ++i)
      assert ((d[i] & 0x80) != 0);

    assert (d[10] == 0x01);

    pb::reader r (w.view ());
    assert (r.next () && r.read_int32 () == -1);
  }

  void
  test_nested_message ()
  {
    pb::writer inner;
    inner.add_uint32 (1, 99u);
    inner.add_string (2, "nested");

    pb::writer outer;
    outer.add_uint32 (1, 1u);
    outer.add_message (2, inner);

    pb::reader r (outer.view ());

    assert (r.next () && r.read_uint32 () == 1u);
    assert (r.next () && r.field_number () == 2);

    pb::reader n (r.read_message ());

    assert (n.next () && n.read_uint32 () == 99u);
    assert (n.next () && n.read_string () == "nested");
    assert (!n.next ());
    assert (!r.next ());
  }

  void
  test_packed_repeated ()
  {
    const uint32_t vs[] = {1u, 300u, 70000u, 0u};

    pb::writer w;
    w.add_packed_uint32 (5, span<const uint32_t> (vs, 4));

    pb::reader r (w.view ());

    assert (r.next () && r.field_number () == 5);
    assert (r.type () == pb::wire_type::length_delimited);

    vector<uint32_t> out;
    r.read_packed_uint32 (out);

    assert (out.size () == 4);
    assert (out[0] == 1u && out[1] == 300u && out[2] == 70000u &&
            out[3] == 0u);
  }

  void
  test_skip_unknown_fields ()
  {
    pb::writer w;

    w.add_uint64 (1, 1ull << 62);
    w.add_fixed64 (2, 7ull);
    w.add_fixed32 (3, 7u);
    w.add_string (4, "skipped");
    w.add_uint32 (5, 12345u);

    pb::reader r (w.view ());

    for (int i (0); i != 4; ++i)
    {
      assert (r.next ());
      r.skip ();
    }

    assert (r.next () && r.field_number () == 5 &&
            r.read_uint32 () == 12345u);
    assert (!r.next ());
  }

  void
  test_truncation_is_rejected ()
  {
    check_rejects (byte_buffer {0x08, 0x80},
                   [] (pb::reader& r) { r.next (); r.read_uint64 (); });

    check_rejects (byte_buffer {0x12, 0x7f, 0x01, 0x02},
                   [] (pb::reader& r) { r.next (); r.read_bytes (); });

    check_rejects (byte_buffer {0x09, 1, 2, 3, 4},
                   [] (pb::reader& r) { r.next (); r.read_fixed64 (); });

    check_rejects (byte_buffer {0x80}, [] (pb::reader& r) { r.next (); });
  }

  void
  test_malformed_tags_are_rejected ()
  {
    check_rejects (byte_buffer {0x00, 0x01},
                   [] (pb::reader& r) { r.next (); });

    check_rejects (byte_buffer {0x0b}, [] (pb::reader& r) { r.next (); });
    check_rejects (byte_buffer {0x0c}, [] (pb::reader& r) { r.next (); });

    check_rejects (byte_buffer {0x0e}, [] (pb::reader& r) { r.next (); });
    check_rejects (byte_buffer {0x0f}, [] (pb::reader& r) { r.next (); });

    check_rejects (byte_buffer {0x08, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
                                0x80, 0x80, 0x80, 0x80, 0x01},
                   [] (pb::reader& r) { r.next (); r.read_uint64 (); });

    check_rejects (byte_buffer {0x08, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
                                0x80, 0x80, 0x80, 0x02},
                   [] (pb::reader& r) { r.next (); r.read_uint64 (); });
  }

  void
  test_narrowing_is_rejected ()
  {
    pb::writer w;
    w.add_uint64 (1, 1ull << 40);

    check_rejects (w.data (), [] (pb::reader& r)
    {
      r.next ();
      r.read_uint32 ();
    });
  }

  void
  test_wire_type_mismatch_is_rejected ()
  {
    pb::writer w;
    w.add_string (1, "not a number");

    check_rejects (w.data (), [] (pb::reader& r)
    {
      r.next ();
      r.read_uint64 ();
    });
  }

  void
  test_reader_contract_is_enforced ()
  {
    pb::writer w;
    w.add_uint32 (1, 1u);
    w.add_uint32 (2, 2u);

    check_rejects (w.data (), [] (pb::reader& r)
    {
      r.next ();
      r.next ();
    });

    check_rejects (w.data (), [] (pb::reader& r)
    {
      r.next ();
      r.read_uint32 ();
      r.read_uint32 ();
    });

    check_rejects (w.data (), [] (pb::reader& r) { r.read_uint32 (); });
  }

  void
  test_empty_buffer ()
  {
    pb::reader r (byte_view {});

    assert (!r.next ());
    assert (r.exhausted ());
  }

  void
  test_zigzag ()
  {
    assert (pb::zigzag_encode (0) == 0u);
    assert (pb::zigzag_encode (-1) == 1u);
    assert (pb::zigzag_encode (1) == 2u);
    assert (pb::zigzag_encode (-2) == 3u);

    const int64_t vs[] = {0,
                          -1,
                          1,
                          -2147483648ll,
                          2147483647ll,
                          numeric_limits<int64_t>::min (),
                          numeric_limits<int64_t>::max ()};

    for (int64_t v : vs)
      assert (pb::zigzag_decode (pb::zigzag_encode (v)) == v);
  }
}

int
main ()
{
  test_varint_roundtrip ();
  test_scalar_roundtrip ();
  test_negative_int32_sign_extension ();
  test_nested_message ();
  test_packed_repeated ();
  test_skip_unknown_fields ();
  test_truncation_is_rejected ();
  test_malformed_tags_are_rejected ();
  test_narrowing_is_rejected ();
  test_wire_type_mismatch_is_rejected ();
  test_reader_contract_is_enforced ();
  test_empty_buffer ();
  test_zigzag ();
}
