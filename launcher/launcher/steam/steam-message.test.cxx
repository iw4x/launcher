#include <launcher/steam/steam-message.hxx>

#include <cassert>
#include <cstdint>
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

  void
  put_u32 (byte_buffer& b, uint32_t v)
  {
    b.push_back (static_cast<uint8_t> (v & 0xff));
    b.push_back (static_cast<uint8_t> ((v >> 8) & 0xff));
    b.push_back (static_cast<uint8_t> ((v >> 16) & 0xff));
    b.push_back (static_cast<uint8_t> ((v >> 24) & 0xff));
  }

  template <typename F>
  void
  check_rejects (F f)
  {
    bool threw (false);

    try
    {
      f ();
    }
    catch (const steam_protocol_error&)
    {
      threw = true;
    }

    assert (threw);
  }

  void
  test_header_roundtrip ()
  {
    steam_message_header h;

    h.steam_id        = 76561198000000001ull;
    h.session_id      = 12345;
    h.job_id_source   = 42;
    h.target_job_name = "Authentication.PollAuthSessionStatus#1";
    h.result          = steam_result::ok;
    h.error_message   = "none";

    byte_buffer d (h.encode ());

    steam_message_header r (steam_message_header::decode (view (d)));

    assert (r.steam_id == h.steam_id);
    assert (r.session_id == h.session_id);
    assert (r.job_id_source == h.job_id_source);
    assert (!r.job_id_target);
    assert (r.target_job_name == h.target_job_name);
    assert (r.result == h.result);
    assert (r.error_message == h.error_message);
  }

  void
  test_sentinel_job_id_is_absent ()
  {
    pb::writer w;

    w.add_fixed64 (10, invalid_job_id);
    w.add_fixed64 (11, invalid_job_id);

    byte_buffer d (w.release ());

    steam_message_header h (steam_message_header::decode (view (d)));

    assert (!h.job_id_source);
    assert (!h.job_id_target);
  }

  void
  test_absent_result_defaults_to_ok ()
  {
    steam_message_header h;

    assert (!h.result);
    assert (h.result_or_ok () == steam_result::ok);

    h.result = steam_result::invalid;
    assert (h.result_or_ok () == steam_result::invalid);
  }

  void
  test_message_roundtrip ()
  {
    pb::writer b;
    b.add_uint32 (1, 65581);

    steam_message m (steam_emsg::client_hello, b.release ());

    m.header.session_id    = 7;
    m.header.job_id_source = 3;

    byte_buffer d (m.encode ());

    assert ((d[3] & 0x80) != 0);

    assert (steam_message::peek (view (d)) == steam_emsg::client_hello);

    steam_message r (steam_message::decode (view (d)));

    assert (r.msg == steam_emsg::client_hello);
    assert (r.header.session_id == 7);
    assert (r.header.job_id_source == 3);

    assert (r.body.size () == 4);
    assert (r.body == m.body);
  }

  void
  test_short_and_malformed_packets ()
  {
    check_rejects ([] { steam_message::peek (byte_view {}); });

    check_rejects ([]
    {
      byte_buffer d;
      put_u32 (d, static_cast<uint32_t> (steam_emsg::client_hello) |
                    emsg_protobuf_flag);

      steam_message::decode (view (d));
    });

    check_rejects ([]
    {
      byte_buffer d;
      put_u32 (d, 751);
      put_u32 (d, 0);

      steam_message::decode (view (d));
    });

    check_rejects ([]
    {
      byte_buffer d;
      put_u32 (d, static_cast<uint32_t> (steam_emsg::client_hello) |
                    emsg_protobuf_flag);
      put_u32 (d, 0xffffffffu);

      steam_message::decode (view (d));
    });
  }

  void
  test_multi_split ()
  {
    byte_buffer p;

    const char* parts[] = {"abc", "de", "f"};

    for (const char* s : parts)
    {
      string t (s);
      put_u32 (p, static_cast<uint32_t> (t.size ()));
      p.insert (p.end (), t.begin (), t.end ());
    }

    vector<byte_view> r (split_multi_payload (view (p)));

    assert (r.size () == 3);
    assert (r[0].size () == 3 && r[1].size () == 2 && r[2].size () == 1);

    assert (split_multi_payload (byte_view {}).empty ());
  }

  void
  test_multi_split_rejects_malformed ()
  {
    check_rejects ([]
    {
      byte_buffer p {0x01, 0x02};
      split_multi_payload (view (p));
    });

    check_rejects ([]
    {
      byte_buffer p;
      put_u32 (p, 100);
      p.push_back (0x01);

      split_multi_payload (view (p));
    });

    check_rejects ([]
    {
      byte_buffer p;
      put_u32 (p, 0);

      split_multi_payload (view (p));
    });
  }

  void
  test_multi_body_decode ()
  {
    pb::writer w;

    const uint8_t payload[] = {1, 2, 3, 4};

    w.add_uint32 (1, 1024);
    w.add_bytes (2, byte_view (payload, sizeof (payload)));

    byte_buffer d (w.release ());

    steam_multi_body m (steam_multi_body::decode (view (d)));

    assert (m.size_unzipped == 1024);
    assert (m.payload.size () == 4);
    assert (m.payload[3] == 4);

    pb::writer u;
    u.add_bytes (2, byte_view (payload, sizeof (payload)));

    byte_buffer e (u.release ());

    steam_multi_body n (steam_multi_body::decode (view (e)));

    assert (n.size_unzipped == 0);
    assert (n.payload.size () == 4);
  }

  void
  test_unknown_header_fields_are_ignored ()
  {
    pb::writer w;

    w.add_fixed64 (1, 99ull);
    w.add_uint32 (3, 10190);
    w.add_string (34, "debug source");
    w.add_int32 (13, 1);

    byte_buffer d (w.release ());

    steam_message_header h (steam_message_header::decode (view (d)));

    assert (h.steam_id == 99ull);
    assert (h.result == steam_result::ok);
  }

  void
  test_emsg_names ()
  {
    assert (to_string (steam_emsg::client_logon) == "ClientLogon");
    assert (to_string (steam_emsg::multi) == "Multi");

    assert (to_string (static_cast<steam_emsg> (4242)) == "EMsg(4242)");
  }
}

int
main ()
{
  test_header_roundtrip ();
  test_sentinel_job_id_is_absent ();
  test_absent_result_defaults_to_ok ();
  test_message_roundtrip ();
  test_short_and_malformed_packets ();
  test_multi_split ();
  test_multi_split_rejects_malformed ();
  test_multi_body_decode ();
  test_unknown_header_fields_are_ignored ();
  test_emsg_names ();
}
