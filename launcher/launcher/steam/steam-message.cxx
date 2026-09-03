#include <launcher/steam/steam-message.hxx>

#include <cassert>
#include <cstring>
#include <limits>

using namespace std;

namespace launcher
{
  string
  to_string (steam_emsg m)
  {
    switch (m)
    {
    case steam_emsg::invalid:
      return "Invalid";
    case steam_emsg::multi:
      return "Multi";
    case steam_emsg::service_method_response:
      return "ServiceMethodResponse";
    case steam_emsg::service_method_call_from_client:
      return "ServiceMethodCallFromClient";
    case steam_emsg::client_heart_beat:
      return "ClientHeartBeat";
    case steam_emsg::client_log_on_response:
      return "ClientLogOnResponse";
    case steam_emsg::client_logged_off:
      return "ClientLoggedOff";
    case steam_emsg::client_license_list:
      return "ClientLicenseList";
    case steam_emsg::client_get_depot_decryption_key:
      return "ClientGetDepotDecryptionKey";
    case steam_emsg::client_get_depot_decryption_key_response:
      return "ClientGetDepotDecryptionKeyResponse";
    case steam_emsg::client_logon:
      return "ClientLogon";
    case steam_emsg::service_method_call_from_client_non_authed:
      return "ServiceMethodCallFromClientNonAuthed";
    case steam_emsg::client_hello:
      return "ClientHello";
    }

    return "EMsg(" + std::to_string (static_cast<uint32_t> (m)) + ")";
  }

  steam_protocol_error::
  steam_protocol_error (const string& w)
    : runtime_error (w)
  {
  }

  namespace
  {
    enum : uint32_t
    {
      header_steam_id        = 1,
      header_session_id      = 2,
      header_job_id_source   = 10,
      header_job_id_target   = 11,
      header_target_job_name = 12,
      header_result          = 13,
      header_error_message   = 14
    };

    enum : uint32_t
    {
      multi_size_unzipped = 1,
      multi_message_body  = 2
    };

    void
    put_u32_le (pb::byte_buffer& b, uint32_t v)
    {
      b.push_back (static_cast<uint8_t> (v & 0xff));
      b.push_back (static_cast<uint8_t> ((v >> 8) & 0xff));
      b.push_back (static_cast<uint8_t> ((v >> 16) & 0xff));
      b.push_back (static_cast<uint8_t> ((v >> 24) & 0xff));
    }

    uint32_t
    get_u32_le (pb::byte_view b, size_t o) noexcept
    {
      assert (o + 4 <= b.size ());

      return static_cast<uint32_t> (b[o]) |
             static_cast<uint32_t> (b[o + 1]) << 8 |
             static_cast<uint32_t> (b[o + 2]) << 16 |
             static_cast<uint32_t> (b[o + 3]) << 24;
    }
  }

  pb::byte_buffer steam_message_header::
  encode () const
  {
    pb::writer w (64);

    if (steam_id)
      w.add_fixed64 (header_steam_id, *steam_id);

    if (session_id)
      w.add_int32 (header_session_id, *session_id);

    if (job_id_source)
      w.add_fixed64 (header_job_id_source, *job_id_source);

    if (job_id_target)
      w.add_fixed64 (header_job_id_target, *job_id_target);

    if (target_job_name)
      w.add_string (header_target_job_name, *target_job_name);

    if (result)
      w.add_int32 (header_result, static_cast<int32_t> (*result));

    if (error_message)
      w.add_string (header_error_message, *error_message);

    return w.release ();
  }

  steam_message_header steam_message_header::
  decode (pb::byte_view d)
  {
    steam_message_header h;

    try
    {
      pb::reader r (d);

      while (r.next ())
      {
        switch (r.field_number ())
        {
        case header_steam_id:
          h.steam_id = r.read_fixed64 ();
          break;

        case header_session_id:
          h.session_id = r.read_int32 ();
          break;

        case header_job_id_source:
          {
            uint64_t v (r.read_fixed64 ());

            if (v != invalid_job_id)
              h.job_id_source = v;

            break;
          }

        case header_job_id_target:
          {
            uint64_t v (r.read_fixed64 ());

            if (v != invalid_job_id)
              h.job_id_target = v;

            break;
          }

        case header_target_job_name:
          h.target_job_name = string (r.read_string ());
          break;

        case header_result:
          h.result = static_cast<steam_result> (r.read_int32 ());
          break;

        case header_error_message:
          h.error_message = string (r.read_string ());
          break;

        default:

          r.skip ();
          break;
        }
      }
    }
    catch (const pb::decode_error& e)
    {
      throw steam_protocol_error (string ("malformed message header: ") +
                                  e.what ());
    }

    return h;
  }

  pb::byte_buffer steam_message::
  encode () const
  {
    assert (msg != steam_emsg::invalid && "refusing to send an invalid emsg");

    pb::byte_buffer h (header.encode ());

    if (h.size () > numeric_limits<uint32_t>::max ())
      throw steam_protocol_error ("message header is too large to frame");

    pb::byte_buffer r;
    r.reserve (8 + h.size () + body.size ());

    put_u32_le (r, static_cast<uint32_t> (msg) | emsg_protobuf_flag);
    put_u32_le (r, static_cast<uint32_t> (h.size ()));

    r.insert (r.end (), h.begin (), h.end ());
    r.insert (r.end (), body.begin (), body.end ());

    return r;
  }

  steam_emsg steam_message::
  peek (pb::byte_view d)
  {
    if (d.size () < 4)
      throw steam_protocol_error ("packet is " + std::to_string (d.size ()) +
                                  " bytes, too short to carry a message id");

    return static_cast<steam_emsg> (get_u32_le (d, 0) & emsg_mask);
  }

  steam_message steam_message::
  decode (pb::byte_view d)
  {
    if (d.size () < 8)
      throw steam_protocol_error ("packet is " + std::to_string (d.size ()) +
                                  " bytes, too short to carry a protobuf "
                                  "message frame");

    uint32_t raw (get_u32_le (d, 0));

    if ((raw & emsg_protobuf_flag) == 0)
      throw steam_protocol_error (
        "received a non-protobuf message (EMsg " +
        std::to_string (raw & emsg_mask) +
        "), which this client does not implement");

    uint32_t hn (get_u32_le (d, 4));

    if (hn > d.size () - 8)
      throw steam_protocol_error (
        "message header length " + std::to_string (hn) +
        " exceeds the " + std::to_string (d.size () - 8) +
        " bytes remaining in the packet");

    steam_message m;

    m.msg    = static_cast<steam_emsg> (raw & emsg_mask);
    m.header = steam_message_header::decode (d.subspan (8, hn));

    pb::byte_view b (d.subspan (8 + hn));
    m.body.assign (b.begin (), b.end ());

    return m;
  }

  steam_multi_body steam_multi_body::
  decode (pb::byte_view d)
  {
    steam_multi_body m;

    try
    {
      pb::reader r (d);

      while (r.next ())
      {
        switch (r.field_number ())
        {
        case multi_size_unzipped:
          m.size_unzipped = r.read_uint32 ();
          break;

        case multi_message_body:
          {
            pb::byte_view b (r.read_bytes ());
            m.payload.assign (b.begin (), b.end ());
            break;
          }

        default:
          r.skip ();
          break;
        }
      }
    }
    catch (const pb::decode_error& e)
    {
      throw steam_protocol_error (string ("malformed multi message: ") +
                                  e.what ());
    }

    return m;
  }

  vector<pb::byte_view>
  split_multi_payload (pb::byte_view p)
  {
    vector<pb::byte_view> r;

    size_t o (0);

    while (o != p.size ())
    {
      if (p.size () - o < 4)
        throw steam_protocol_error (
          "multi payload has " + std::to_string (p.size () - o) +
          " trailing bytes, too few for a packet length prefix");

      uint32_t n (get_u32_le (p, o));
      o += 4;

      if (n > p.size () - o)
        throw steam_protocol_error (
          "multi payload declares a " + std::to_string (n) +
          " byte packet but only " + std::to_string (p.size () - o) +
          " bytes remain");

      if (n == 0)
        throw steam_protocol_error ("multi payload contains an empty packet");

      r.push_back (p.subspan (o, n));
      o += n;
    }

    return r;
  }
}
