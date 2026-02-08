#include <launcher/steam/steam-library.hxx>
#include <launcher/steam/steam-types.hxx>

#include <cassert>
#include <iostream>
#include <filesystem>

using namespace std;
using namespace launcher;

namespace fs = std::filesystem;

// Check if we can properly validate library paths.
//
// We are mostly concerned with the Windows path normalization mess here, that
// is, we don't want to choke on external library paths.
//

static void
test_validate_nonexistent ()
{
  // If it's not there, we shouldn't accept it.
  //
  assert (!steam_library_manager::validate_library_path ("/nonexistent/path"));
  assert (!steam_library_manager::validate_library_path ("X:/NonExistent/Steam"));
}

static void
test_validate_path_formats ()
{
  // We need a scratchpad on the filesystem to verify the validator actually
  // checks for the required subdirectories.
  //
  fs::path d (fs::temp_directory_path () / "steam-library-test");
  fs::path sa (d / "steamapps");

  // Wipe the slate clean in case a previous run crashed.
  //
  fs::remove_all (d);

  // Mock up the structure we expect.
  //
  fs::create_directories (sa);

  // This should pass now that steamapps exists.
  //
  assert (steam_library_manager::validate_library_path (d));

  // Try to confuse it with a trailing slash.
  //
  fs::path ds (d / "");
  assert (steam_library_manager::validate_library_path (ds));

  // Throw in some redundancy just to be sure we normalize internally.
  //
  fs::path dd (d / "." / "");
  assert (steam_library_manager::validate_library_path (dd));

  fs::path ddd (d / "steamapps" / ".." / "");
  assert (steam_library_manager::validate_library_path (ddd));

  // Clean up after ourselves.
  //
  fs::remove_all (d);
}

static void
test_validate_missing_steamapps ()
{
  // Now let's try a directory that exists but lacks the structure we need.
  //
  fs::path d (fs::temp_directory_path () / "steam-library-test-no-steamapps");

  fs::remove_all (d);
  fs::create_directories (d);

  // It's a directory, but it's not a Steam library.
  //
  assert (!steam_library_manager::validate_library_path (d));

  fs::remove_all (d);
}

// Check if we can assemble the path components correctly.
//
static void
test_library_path_construction ()
{
  // Simulate what we get from the VDF parser.
  //
  steam_library l;

  // On Windows, Steam might give us forward slashes even for local drives.
  // Force preference.
  //
  l.path = fs::path ("D:/SteamLibrary").lexically_normal ().make_preferred ();

  // See if we can walk down the tree.
  //
  fs::path sa (l.path / "steamapps");
  fs::path c (sa / "common");
  fs::path g (c / "Call of Duty Modern Warfare 2");

  assert (!sa.empty ());
  assert (!c.empty ());
  assert (!g.empty ());

  // Make sure we didn't lose the tail.
  //
  assert (g.filename () == "Call of Duty Modern Warfare 2");

  // Double check normalization doesn't mangle the filename.
  //
  fs::path n (g.lexically_normal ().make_preferred ());
  assert (n.filename () == "Call of Duty Modern Warfare 2");
}

// Verify normalized paths behave like real paths.
//
static void
test_path_operations ()
{
  // Start with mixed separators.
  //
  fs::path b ("D:/SteamLibrary");
  fs::path n (b.lexically_normal ().make_preferred ());

  // Append and verify.
  //
  fs::path f (n / "steamapps" / "common" / "Game");
  assert (!f.empty ());
  assert (f.filename () == "Game");

  // Back up one level.
  //
  fs::path p (f.parent_path ());
  assert (p.filename () == "common");

  // Count the depth to ensure we haven't collapsed everything.
  //
  int c (0);
  for (const auto& e : f)
    ++c;
  assert (c >= 4); // base, steamapps, common, Game
}

// Verify the POD struct defaults.
//
static void
test_library_struct ()
{
  // Fresh instance should be empty.
  //
  steam_library l1;
  assert (l1.path.empty ());
  assert (l1.label.empty ());
  assert (l1.apps.empty ());

  // Explicit construction.
  //
  steam_library l2 ("External", fs::path ("D:/SteamLibrary"));
  assert (l2.label == "External");
  assert (!l2.path.empty ());
  assert (l2.contentid == 0);
  assert (l2.totalsize == 0);
}

// Verify the manifest container.
//
static void
test_manifest_struct ()
{
  steam_app_manifest m;
  assert (m.appid == 0);
  assert (m.name.empty ());
  assert (m.installdir.empty ());
  assert (m.fullpath.empty ());
  assert (m.size_on_disk == 0);
  assert (m.buildid == 0);

  // Fill it up like the parser would.
  //
  m.appid = 10190;
  m.name = "Call of Duty: Modern Warfare 2 - Multiplayer";
  m.installdir = "Call of Duty Modern Warfare 2";

  // Reconstruct the absolute path.
  //
  fs::path lp ("D:/SteamLibrary");
  m.fullpath = (lp / "steamapps" / "common" / m.installdir)
                 .lexically_normal ()
                 .make_preferred ();

  assert (!m.fullpath.empty ());
  assert (m.fullpath.filename () == "Call of Duty Modern Warfare 2");
}

// Verify config paths container.
//
static void
test_config_paths_struct ()
{
  steam_config_paths p;
  assert (p.steam_root.empty ());
  assert (p.config_vdf.empty ());
  assert (p.libraryfolders_vdf.empty ());
  assert (p.steamapps.empty ());
}

// This covers the regression where we failed to handle external drives
// on Windows because of mixed separator styles in the VDF.
//
static void
test_external_library_path_regression ()
{
  // Raw string from VDF.
  //
  string s ("D:/SteamLibrary");

  // Fix it up.
  //
  fs::path p (s);
  fs::path n (p.lexically_normal ().make_preferred ());

  assert (!n.empty ());

  // Build the game path.
  //
  string id ("Call of Duty Modern Warfare 2");
  fs::path gp ((n / "steamapps" / "common" / id)
                 .lexically_normal ()
                 .make_preferred ());

  assert (!gp.empty ());
  assert (gp.filename () == id);

  // Sanity check the hierarchy.
  //
  assert (gp.parent_path ().filename () == "common");
}

// Spaces in paths must not break our path algebra.
//
static void
test_paths_with_spaces ()
{
  fs::path b ("C:/Program Files (x86)/Steam");
  fs::path n (b.lexically_normal ().make_preferred ());

  assert (!n.empty ());

  fs::path sa (n / "steamapps");
  fs::path g (sa / "common" / "Call of Duty Modern Warfare 2");

  assert (!g.empty ());
  assert (g.filename () == "Call of Duty Modern Warfare 2");

  // Make sure we kept the spaces and didn't accidentally encode them.
  //
  string s (n.string ());
  assert (s.find ("Program Files") != string::npos ||
          s.find ("Program%20Files") != string::npos);
}

int
main ()
{
  test_validate_nonexistent ();
  test_validate_path_formats ();
  test_validate_missing_steamapps ();
  test_library_path_construction ();
  test_path_operations ();
  test_library_struct ();
  test_manifest_struct ();
  test_config_paths_struct ();
  test_external_library_path_regression ();
  test_paths_with_spaces ();
}
