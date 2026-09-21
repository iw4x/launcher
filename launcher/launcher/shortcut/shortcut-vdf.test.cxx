#include <launcher/shortcut/shortcut-vdf.hxx>

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
using namespace launcher;

static vector<char>
bytes (initializer_list<int> l)
{
  vector<char> r;
  r.reserve (l.size ());

  for (int b : l)
    r.push_back (static_cast<char> (b));

  return r;
}

static void
append (vector<char>& d, const string& s)
{
  d.insert (d.end (), s.begin (), s.end ());
  d.push_back ('\0');
}

static void
check_round_trip (const vector<char>& d)
{
  assert (vdf_object::parse (d).serialize () == d);
}

static void
check_reject (const vector<char>& d)
{
  try
  {
    vdf_object::parse (d);
  }
  catch (const runtime_error&)
  {
    return;
  }

  assert (false);
}

static vector<char>
sample ()
{
  vector<char> d;

  d.push_back (0x00);
  append (d, "shortcuts");

  for (int i (0); i != 2; ++i)
  {
    d.push_back (0x00);
    append (d, to_string (i));

    d.push_back (0x02);
    append (d, "appid");
    for (int b : {0x11, 0x22, 0x33, 0x84})
      d.push_back (static_cast<char> (b));

    d.push_back (0x01);
    append (d, "AppName");
    append (d, "Some Game " + to_string (i));

    d.push_back (0x01);
    append (d, "Exe");
    append (d, "\"/opt/game/run\"");

    d.push_back (0x01);
    append (d, "LaunchOptions");
    append (d, "");

    d.push_back (0x00);
    append (d, "tags");
    d.push_back (0x08);

    d.push_back (0x08);
  }

  d.push_back (0x08);
  d.push_back (0x08);

  return d;
}

int
main ()
{
  check_round_trip (bytes ({0x08}));
  assert (vdf_object::parse (bytes ({0x08})).empty ());

  check_round_trip (sample ());

  {
    vdf_object d (vdf_object::parse (sample ()));

    const vdf_object* s (d.object ("shortcuts"));
    assert (s != nullptr);
    assert (s->size () == 2);
    assert (s->keys () == vector<string> ({"0", "1"}));

    const vdf_object* e (s->object ("1"));
    assert (e != nullptr);

    const string* n (e->string ("AppName"));
    assert (n != nullptr && *n == "Some Game 1");

    const string* o (e->string ("LaunchOptions"));
    assert (o != nullptr && o->empty ());

    const int32_t* a (e->number ("appid"));
    assert (a != nullptr);
    assert (*a == static_cast<int32_t> (0x84332211u));
    assert (*a < 0);

    assert (e->object ("tags") != nullptr);
    assert (e->object ("tags")->empty ());

    assert (e->number ("AppName") == nullptr);
    assert (e->string ("appid") == nullptr);
    assert (e->object ("AppName") == nullptr);
    assert (e->string ("NoSuchKey") == nullptr);
  }

  {
    vdf_object d (vdf_object::parse (sample ()));
    vdf_object* s (d.object ("shortcuts"));
    vdf_object* e (s->object ("0"));

    e->set (string ("AppName"), string ("Renamed"));
    e->set (string ("appid"), static_cast<int32_t> (-2));

    assert (e->keys () == vector<string> (
              {"appid", "AppName", "Exe", "LaunchOptions", "tags"}));

    assert (*e->string ("AppName") == "Renamed");
    assert (*e->number ("appid") == -2);

    e->set (string ("FlatpakAppID"), string ());
    assert (e->keys ().back () == "FlatpakAppID");

    e->set (string ("tags"), string ("gone"));
    assert (e->object ("tags") == nullptr);
    assert (*e->string ("tags") == "gone");

    check_round_trip (d.serialize ());

    e->erase ("tags");
    assert (e->string ("tags") == nullptr);
  }

  check_reject ({});
  check_reject (bytes ({0x01}));
  check_reject (bytes ({0x00, 'a', 0x00}));
  check_reject (bytes ({0x01, 'a', 0x00, 'b'}));
  check_reject (bytes ({0x02, 'a', 0x00, 1, 2}));
  check_reject (bytes ({0x07, 'a', 0x00, 0x08}));
  check_reject (bytes ({0x08, 0x08}));

  {
    vdf_object d;
    d.set (string ("k"), string ("has\0nul", 7));

    try
    {
      d.serialize ();
      assert (false);
    }
    catch (const runtime_error&)
    {
    }
  }

  return 0;
}
