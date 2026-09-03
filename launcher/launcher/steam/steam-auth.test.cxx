#include <launcher/steam/steam-auth.hxx>

#include <cassert>
#include <string>

using namespace std;
using namespace launcher;

namespace
{
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

  const char steam_token[] =
    "eyJ0eXAiOiJKV1QiLCJhbGciOiJFZERTQSJ9."
    "eyJpc3MiOiJzdGVhbSIsInN1YiI6Ijc2NTYxMTk4MDAwMDAwMDAxIiwiYXVkIjpbImNsaWVu"
    "dCJdfQ."
    "c2lnbmF0dXJlLWJ5dGVz";

  void
  test_steam_id_from_token ()
  {
    assert (steam_id_from_token (steam_token) == 76561198000000001ull);
  }

  void
  test_url_safe_alphabet ()
  {
    const char t[] =
      "aGVhZGVy."
      "eyJzdWIiOiI3NjU2MTE5ODAwMDAwMDAwMiIsIngiOiJhP34-In0."
      "c2ln";

    assert (steam_id_from_token (t) == 76561198000000002ull);
  }

  void
  test_malformed_tokens_are_rejected ()
  {
    check_rejects ([] { steam_id_from_token (""); });
    check_rejects ([] { steam_id_from_token ("not-a-token"); });
    check_rejects ([] { steam_id_from_token ("only.two"); });

    check_rejects ([] { steam_id_from_token ("a..c"); });

    check_rejects ([] { steam_id_from_token ("a.!!!!.c"); });

    check_rejects ([] { steam_id_from_token ("a.bm90IGpzb24.c"); });

    check_rejects ([]
    {
      steam_id_from_token ("a.eyJpc3MiOiJzdGVhbSJ9.c");
    });

    check_rejects ([]
    {
      steam_id_from_token ("a.eyJzdWIiOiJub3QtYS1udW1iZXIifQ.c");
    });
  }

  void
  test_guard_type_names ()
  {
    assert (to_string (steam_guard_type::device_code) == "device code");
    assert (to_string (steam_guard_type::email_code) == "email code");

    assert (to_string (static_cast<steam_guard_type> (99)) ==
            "EAuthSessionGuardType(99)");
  }

  void
  test_requires_code ()
  {
    assert (requires_code (steam_guard_type::device_code));
    assert (requires_code (steam_guard_type::email_code));

    assert (!requires_code (steam_guard_type::device_confirmation));
    assert (!requires_code (steam_guard_type::email_confirmation));
    assert (!requires_code (steam_guard_type::machine_token));
    assert (!requires_code (steam_guard_type::none));
    assert (!requires_code (steam_guard_type::unknown));
  }
}

int
main ()
{
  test_steam_id_from_token ();
  test_url_safe_alphabet ();
  test_malformed_tokens_are_rejected ();
  test_guard_type_names ();
  test_requires_code ();
}
