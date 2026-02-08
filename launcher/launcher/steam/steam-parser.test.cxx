#include <launcher/steam/steam-parser.hxx>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <sstream>

#include <launcher/steam/steam-types.hxx>

using namespace std;
using namespace launcher;

namespace fs = std::filesystem;

// Basics. Check that we can parse a simple key-value pair and a nested
// object structure.
//
static void
test_basic ()
{
  // Simple pair.
  //
  {
    string s (R"("key" "value")");
    auto n (vdf_parser::parse (s));

    assert (n.is_object ());
    assert (n.get_string ("key") == "value");
  }

  // Nested.
  //
  {
    string s (R"(
      "root"
      {
        "child" "value"
      }
    )");

    auto n (vdf_parser::parse (s));
    assert (n.is_object ());

    auto* r (n.get_object ("root"));
    assert (r != nullptr);

    auto i (r->find ("child"));
    assert (i != r->end ());
    assert (i->second.is_string ());
    assert (i->second.as_string () == "value");
  }
}

// Escaping. VDF uses backslashes, and since we are dealing with Windows
// paths, we are going to see a lot of them.
//
static void
test_escapes ()
{
  // Windows paths (single backslash in logic, double in C++ string).
  //
  {
    string s (R"("path" "C:\\Program Files\\Steam")");
    auto n (vdf_parser::parse (s));

    assert (n.get_string ("path") == "C:\\Program Files\\Steam");
  }

  // Double-escaped. This is how they actually appear in the file.
  //
  {
    string s (R"("path" "D:\\SteamLibrary")");
    auto n (vdf_parser::parse (s));

    assert (n.get_string ("path") == "D:\\SteamLibrary");
  }

  // Newlines and tabs.
  //
  {
    string s (R"("text" "line1\nline2\ttab")");
    auto n (vdf_parser::parse (s));

    assert (n.get_string ("text") == "line1\nline2\ttab");
  }
}

// Library folders. This is the tricky one. On Windows, Steam insists on
// writing paths with forward slashes in libraryfolders.vdf, which makes
// std::filesystem unhappy if we aren't careful.
//
static void
test_library_paths ()
{
  // Simulate the Windows format with forward slashes that caused the external
  // library detection failure.
  //
  {
    string s (R"(
      "libraryfolders"
      {
        "0"
        {
          "path"    "C:/Program Files (x86)/Steam"
          "apps"    { "10190" "1234" }
        }
        "1"
        {
          "path"    "D:/SteamLibrary"
          "apps"    { "10190" "8765" }
        }
      }
    )");

    auto n (vdf_parser::parse (s));
    auto* l (n.get_object ("libraryfolders"));
    assert (l != nullptr);

    auto i0 (l->find ("0"));
    auto i1 (l->find ("1"));

    assert (i0 != l->end ());
    assert (i1 != l->end ());

    // Raw paths.
    //
    string s0 (i0->second.get_string ("path"));
    string s1 (i1->second.get_string ("path"));

    assert (s0 == "C:/Program Files (x86)/Steam");
    assert (s1 == "D:/SteamLibrary");

    // Normalize. We need to make sure we convert to native separators
    // (backslashes on Windows) so the rest of the logic works.
    //
    fs::path p0 (s0);
    fs::path p1 (s1);

    fs::path n0 (p0.lexically_normal ().make_preferred ());
    fs::path n1 (p1.lexically_normal ().make_preferred ());

    assert (!n0.empty ());
    assert (!n1.empty ());

    // Components should survive.
    //
    assert (n0.filename () == "Steam" || n0.filename () == "Steam/");
    assert (n1.filename () == "SteamLibrary" || n1.filename () == "SteamLibrary/");
  }
}

// Path normalization specifics. Make sure we handle mixed separators and
// odd dot components correctly.
//
static void
test_normalization ()
{
  // Unix style on Windows.
  //
  {
    fs::path p ("D:/SteamLibrary/steamapps/common");
    fs::path n (p.lexically_normal ().make_preferred ());

    assert (!n.empty ());
    assert (p.lexically_normal () == n.lexically_normal ());
  }

  // Mixed.
  //
  {
    fs::path p ("D:/SteamLibrary\\steamapps/common");
    fs::path n (p.lexically_normal ().make_preferred ());

    assert (!n.empty ());
  }

  // Trailing slash.
  //
  {
    fs::path p ("D:/SteamLibrary/");
    fs::path n (p.lexically_normal ().make_preferred ());

    assert (!n.empty ());
  }

  // Dot cleanup.
  //
  {
    fs::path p ("D:/SteamLibrary/./steamapps/../steamapps/common");
    fs::path n (p.lexically_normal ().make_preferred ());

    assert (!n.empty ());

    string s (n.string ());
    assert (s.find ("..") == string::npos);
  }

  // Concatenation.
  //
  {
    fs::path b ("D:/SteamLibrary");
    fs::path n (b.lexically_normal ().make_preferred ());
    fs::path f (n / "steamapps" / "common" / "MW2");

    assert (!f.empty ());

    // Ensure consistency.
    //
    fs::path fn (f.lexically_normal ().make_preferred ());
    assert (fn == f.lexically_normal ().make_preferred ());
  }
}

// Path comparison. We need to be able to tell if two paths point to the
// same place even if they were constructed differently.
//
static void
test_comparison ()
{
  // Redundant components.
  //
  {
    fs::path p1 ("D:/SteamLibrary/steamapps");
    fs::path p2 ("D:/SteamLibrary/./steamapps");
    fs::path p3 ("D:/SteamLibrary/common/../steamapps");

    fs::path n1 (p1.lexically_normal ());
    fs::path n2 (p2.lexically_normal ());
    fs::path n3 (p3.lexically_normal ());

    assert (n1 == n2);
    assert (n1 == n3);
  }
}

// Realistic data. Parse a full chunk of a real libraryfolders.vdf to ensure
// the recursive structure holds up.
//
static void
test_realistic ()
{
  string s (R"(
"libraryfolders"
{
  "0"
  {
    "path"    "C:\\Program Files (x86)\\Steam"
    "apps"    { "10190" "4556448768" }
  }
  "1"
  {
    "path"    "D:\\SteamLibrary"
    "apps"    { "10190" "4556448768" }
  }
}
)");

  auto n (vdf_parser::parse (s));
  auto* l (n.get_object ("libraryfolders"));
  assert (l != nullptr);

  auto i0 (l->find ("0"));
  auto i1 (l->find ("1"));

  assert (i0 != l->end ());
  assert (i1 != l->end ());

  // In this case (real file on disk), we might see backslashes.
  //
  string p0 (i0->second.get_string ("path"));
  string p1 (i1->second.get_string ("path"));

  assert (p0 == "C:\\Program Files (x86)\\Steam");
  assert (p1 == "D:\\SteamLibrary");

  // App mapping.
  //
  auto* a0 (i0->second.get_object ("apps"));
  auto* a1 (i1->second.get_object ("apps"));

  assert (a0 != nullptr);
  assert (a1 != nullptr);
  assert (a0->find ("10190") != a0->end ());
}

// App manifest (acf). Check that we can extract the install dir and build
// the full path.
//
static void
test_manifest ()
{
  string s (R"(
"AppState"
{
  "appid"       "10190"
  "name"        "Call of Duty: Modern Warfare 2 - Multiplayer"
  "installdir"  "Call of Duty Modern Warfare 2"
}
)");

  auto n (vdf_parser::parse (s));
  auto* st (n.get_object ("AppState"));
  assert (st != nullptr);

  auto i (st->find ("installdir"));
  assert (i != st->end ());
  assert (i->second.as_string () == "Call of Duty Modern Warfare 2");

  // Construct full path.
  //
  fs::path l ("D:/SteamLibrary");
  string d (i->second.as_string ());

  fs::path f ((l / "steamapps" / "common" / d)
              .lexically_normal ()
              .make_preferred ());

  assert (!f.empty ());
  assert (f.filename () == "Call of Duty Modern Warfare 2");
}

// Comments. The parser shouldn't choke on C-style comments.
//
static void
test_comments ()
{
  string s (R"(
// Header
"libraryfolders"
{
  // Entry
  "0"
  {
    "path" "C:/Steam"  // Inline
  }
}
)");

  auto n (vdf_parser::parse (s));
  auto* l (n.get_object ("libraryfolders"));
  assert (l != nullptr);

  auto i (l->find ("0"));
  assert (i != l->end ());
  assert (i->second.get_string ("path") == "C:/Steam");
}

// Regression. This covers the specific scenario: Steam on C:, Game on D:,
// and paths stored with forward slashes.
//
static void
test_regression_01 ()
{
  string s (R"(
"libraryfolders"
{
  "0"
  {
    "path" "C:/Program Files (x86)/Steam"
    "apps" { "228980" "290" }
  }
  "1"
  {
    "path" "D:/SteamLibrary"
    "apps" { "10190" "455" }
  }
}
)");

  auto n (vdf_parser::parse (s));
  auto* l (n.get_object ("libraryfolders"));
  assert (l != nullptr);

  // Iterate exactly as we do in the actual launcher code.
  //
  for (const auto& [k, v] : *l)
  {
    if (k.empty () || !isdigit (static_cast<unsigned char> (k[0])))
      continue;

    if (!v.is_object ())
      continue;

    auto* p (v.find ("path"));
    if (p == nullptr || !p->is_string ())
      continue;

    // Fix the slashes.
    //
    fs::path fp (fs::path (p->as_string ())
                 .lexically_normal ()
                 .make_preferred ());

    assert (!fp.empty ());

    fs::path g (
      (fp / "steamapps" / "common" / "Call of Duty Modern Warfare 2")
      .lexically_normal ()
      .make_preferred ());

    assert (!g.empty ());
    assert (g.filename () == "Call of Duty Modern Warfare 2");
  }
}

int
main ()
{
  test_basic ();
  test_escapes ();
  test_library_paths ();
  test_normalization ();
  test_comparison ();
  test_realistic ();
  test_manifest ();
  test_comments ();
  test_regression_01 ();
}
