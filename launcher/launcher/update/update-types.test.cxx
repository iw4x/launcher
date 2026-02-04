#include <launcher/update/update-types.hxx>

#include <cassert>
#include <iostream>
#include <sstream>

using namespace std;
using namespace launcher;

// We are dealing with a hybrid of SemVer and build2-specific conventions
// (specifically the .z suffix). The main complexity here is mapping string
// representations (alpha/beta) into comparable integers so we don't have to
// do string parsing during comparison.
//

static void
check (const string& s,
       uint32_t mj,
       uint32_t mi,
       uint32_t pt,
       uint16_t pr = 0,
       uint64_t sn = 0,
       const string& id = "")
{
  auto v (parse_launcher_version (s));

  if (!v)
    assert (false);

  // Check individual components. If we mismatch here, it means the parser
  // state machine is likely misidentifying the delimiter or the numeric
  // conversion is off.
  //
  if (v->major != mj ||
      v->minor != mi ||
      v->patch != pt ||
      v->pre_release != pr ||
      v->snapshot_sn != sn ||
      v->snapshot_id != id)
  {
    assert (false);
  }
}

static void
check_fail (const string& s)
{
  auto v (parse_launcher_version (s));

  if (v)
    assert (false);
}

static void
check_cmp (const launcher_version& l, const launcher_version& r, int e)
{
  int res (l.compare (r));

  // Normalize to -1, 0, 1 for the check.
  //
  if (res < 0) res = -1;
  if (res > 0) res = 1;

  if (res != e)
    assert (false);
}

// Check standard X.Y.Z releases.
//
static void
test_rel ()
{
  // Basic triplets and large integers.
  //
  check ("1.0.0", 1, 0, 0);
  check ("0.1.0", 0, 1, 0);
  check ("0.0.1", 0, 0, 1);
  check ("1.2.3", 1, 2, 3);
  check ("10.20.30", 10, 20, 30);
  check ("99999.99999.99999", 99999, 99999, 99999);

  // Strip the 'v' prefix often found in git tags.
  //
  check ("v1.2.3", 1, 2, 3);
  check ("V1.2.3", 1, 2, 3);
}

// Check pre-release mapping (alpha/beta).
//
// We map these to the pre_release integer field:
//   alpha: 1-499
//   beta:  501-999 (500 + N)
//
static void
test_pre ()
{
  // Alphas.
  //
  check ("1.0.0-a.1", 1, 0, 0, 1);
  check ("1.2.3-a.1", 1, 2, 3, 1);
  check ("1.2.3-a.99", 1, 2, 3, 99);
  check ("1.2.3-a.499", 1, 2, 3, 499);
  check ("v1.2.3-a.1", 1, 2, 3, 1);

  // Betas. Offset by 500 so we can compare straight integers later.
  //
  check ("1.0.0-b.1", 1, 0, 0, 501);
  check ("1.2.3-b.1", 1, 2, 3, 501);
  check ("1.2.3-b.7", 1, 2, 3, 507);
  check ("1.2.3-b.99", 1, 2, 3, 599);
  check ("1.2.3-b.499", 1, 2, 3, 999);
  check ("v1.1.8-b.7", 1, 1, 8, 507);
}

static void
test_snap ()
{
  // Timestamp-based snapshots. We store the timestamp in snapshot_sn.
  //
  check ("1.2.0-a.1.20260201010251", 1, 2, 0, 1, 20260201010251ULL);
  check ("1.2.0-a.1.20260201010251.fe4660334ed0",
         1, 2, 0, 1, 20260201010251ULL, "fe4660334ed0");

  check ("1.2.0-b.1.20260201010251", 1, 2, 0, 501, 20260201010251ULL);
  check ("1.2.0-b.7.20260201010251.abc123",
         1, 2, 0, 507, 20260201010251ULL, "abc123");

  // The .z convention.
  //
  // Development snapshots where we don't have a specific timestamp but need
  // to indicate "post-release". We treat 'z' as 1.
  //
  check ("1.2.0-a.1.z", 1, 2, 0, 1, 1);
  check ("1.2.0-b.1.z", 1, 2, 0, 501, 1);
  check ("1.1.8-b.7.z", 1, 1, 8, 507, 1);
  check ("v1.1.8-b.7.z", 1, 1, 8, 507, 1);
  check ("1.2.0-a.1.Z", 1, 2, 0, 1, 1);
}

static void
test_fail ()
{
  check_fail ("");
  check_fail ("1");
  check_fail ("1.2");
  check_fail ("1.");
  check_fail ("1.2.");
  check_fail (".1.2");

  // Invalid pre-release formats.
  //
  check_fail ("a.b.c");
  check_fail ("1.2.3-x.1");

  // Range checks. Artificial 499 iterations.
  //
  check_fail ("1.2.3-a.500");
  check_fail ("1.2.3-b.500");

  // Invalid snapshot tails.
  //
  check_fail ("1.2.3-a.1.x");
  check_fail ("1.2.3-a.1.abc");
}

static void
test_cmp ()
{
  // Identity.
  //
  check_cmp (launcher_version (1, 2, 3), launcher_version (1, 2, 3), 0);

  // Standard component precedence.
  //
  check_cmp (launcher_version (1, 0, 0), launcher_version (2, 0, 0), -1);
  check_cmp (launcher_version (2, 0, 0), launcher_version (1, 0, 0), 1);
  check_cmp (launcher_version (1, 1, 0), launcher_version (1, 2, 0), -1);
  check_cmp (launcher_version (1, 2, 0), launcher_version (1, 1, 0), 1);
  check_cmp (launcher_version (1, 2, 1), launcher_version (1, 2, 2), -1);
  check_cmp (launcher_version (1, 2, 2), launcher_version (1, 2, 1), 1);

  // Release vs Pre-release.
  //
  // A release is "newer" than its own beta.
  //
  {
    launcher_version rel (1, 2, 3);
    launcher_version alp (1, 2, 3, 1);   // a.1
    launcher_version bet (1, 2, 3, 501); // b.1

    check_cmp (rel, alp, 1);
    check_cmp (rel, bet, 1);
    check_cmp (alp, rel, -1);
    check_cmp (bet, rel, -1);

    // Alpha < Beta.
    //
    check_cmp (alp, bet, -1);
    check_cmp (bet, alp, 1);
  }

  // Pre-release versions.
  //
  {
    launcher_version a1 (1, 2, 3, 1);
    launcher_version a2 (1, 2, 3, 2);
    launcher_version b1 (1, 2, 3, 501);
    launcher_version b2 (1, 2, 3, 502);

    check_cmp (a1, a2, -1);
    check_cmp (b1, b2, -1);
  }

  // Snapshots vs Pre-releases.
  //
  // A snapshot is "newer" than the base pre-release.
  //
  {
    launcher_version b7 (1, 1, 8, 507);         // b.7
    launcher_version b7s (1, 1, 8, 507, 1);     // b.7.z
    launcher_version b7t (1, 1, 8, 507, 20260201010251ULL);

    check_cmp (b7, b7s, -1);
    check_cmp (b7s, b7, 1);
    check_cmp (b7s, b7t, -1);
  }
}

static void
test_str ()
{
  assert (launcher_version (1, 2, 3).string () == "1.2.3");
  assert (launcher_version (1, 2, 3, 1).string () == "1.2.3-a.1");
  assert (launcher_version (1, 2, 3, 99).string () == "1.2.3-a.99");
  assert (launcher_version (1, 2, 3, 501).string () == "1.2.3-b.1");
  assert (launcher_version (1, 2, 3, 507).string () == "1.2.3-b.7");
  assert (launcher_version (1, 2, 0, 1, 20260201010251ULL).string () ==
          "1.2.0-a.1.20260201010251");
  assert (launcher_version (1, 2, 0, 1, 20260201010251ULL, "abc123").string () ==
          "1.2.0-a.1.20260201010251.abc123");
}

// Verify predicates and accessors.
//
static void
test_help ()
{
  launcher_version e;
  launcher_version r (1, 2, 3);
  launcher_version a (1, 2, 3, 1);
  launcher_version b (1, 2, 3, 501);
  launcher_version s (1, 2, 3, 1, 12345);

  assert (e.empty ());
  assert (!r.empty ());

  assert (r.release ());
  assert (!a.release ());

  assert (!r.is_alpha ());
  assert (a.is_alpha ());
  assert (!b.is_alpha ());

  assert (b.is_beta ());
  assert (s.snapshot ());

  // Accessors return optionals.
  //
  assert (!r.alpha ().has_value ());
  assert (a.alpha ().has_value ());
  assert (*a.alpha () == 1);
  assert (*b.beta () == 1);
}

static void
test_loop ()
{
  const char* vs[] = {
    "1.0.0",
    "1.2.3",
    "1.2.3-a.1",
    "1.2.3-b.7",
    "1.2.3-a.1.20260201010251",
    "1.2.3-a.1.20260201010251.abc123"
  };

  for (const char* raw : vs)
  {
    auto p1 (parse_launcher_version (raw));
    assert (p1.has_value ());

    string s (p1->string ());
    auto p2 (parse_launcher_version (s));
    assert (p2.has_value ());

    assert (p1->compare (*p2) == 0);
  }
}

static void
test_regr ()
{
  // We previously failed to compare the .z dev snapshot against the release
  // tag correctly. The .z must be > than the base pre-release.
  //
  //   Current: 1.1.8-b.7.z
  //   Release: v1.1.8-b.7
  //
  // If we get this wrong, we might try to "update" backwards.
  //

  auto cur (parse_launcher_version ("1.1.8-b.7.z"));
  assert (cur.has_value ());
  assert (cur->is_beta ());
  assert (*cur->beta () == 7);
  assert (cur->snapshot_sn == 1);

  auto rel (parse_launcher_version ("v1.1.8-b.7"));
  assert (rel.has_value ());
  assert (rel->is_beta ());
  assert (rel->snapshot_sn == 0);

  // Development snapshot must be newer than the clean tag.
  //
  assert (*cur > *rel);
}

int
main ()
{
  test_rel ();
  test_pre ();
  test_snap ();
  test_fail ();
  test_cmp ();
  test_str ();
  test_help ();
  test_loop ();
  test_regr ();
}
