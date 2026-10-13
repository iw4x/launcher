#include <launcher/launcher-manifest.hxx>

#include <cassert>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>

using namespace std;
using namespace launcher;

// Archive created by Info-ZIP on Linux with zip -X, as the rawfiles release
// script does. It contains run.sh with mode 0755 and data.txt with mode 0644
// (see launcher-manifest.test.zip).
//
static const unsigned char archive_data[] = {
#include <launcher/launcher-manifest.test-zip.hxx>
};

static vector<fs::path>
extract (const fs::path& ap, const fs::path& d)
{
  manifest_archive a;
  a.name = ap.filename ().string ();

  vector<fs::path> r;

  asio::io_context io;
  asio::co_spawn (io,
                  manifest_coordinator::extract_archive (a, ap, d),
                  [&r] (exception_ptr e, vector<fs::path> v)
  {
    if (e)
      rethrow_exception (e);

    r = move (v);
  });

  io.run ();
  return r;
}

int
main ()
{
  fs::path d (fs::temp_directory_path () /
              ("launcher-manifest-test-" +
               to_string (chrono::steady_clock::now ().time_since_epoch ()
                            .count ())));

  fs::create_directories (d);

  fs::path ap (d / "fixture.zip");
  {
    ofstream o (ap, ios::binary);
    o.write (reinterpret_cast<const char*> (archive_data),
             sizeof (archive_data));
  }

  vector<fs::path> r (extract (ap, d / "out"));

  assert (r.size () == 2);
  assert (fs::exists (d / "out" / "run.sh"));
  assert (fs::exists (d / "out" / "data.txt"));

  // The executable bits recorded in the archive survive the extraction and
  // are not added to the other files.
  //
#ifndef _WIN32
  const fs::perms e (fs::perms::owner_exec |
                     fs::perms::group_exec |
                     fs::perms::others_exec);

  auto x ([&e] (const fs::path& f)
  {
    return fs::status (f).permissions () & e;
  });

  assert (x (d / "out" / "run.sh") == e);
  assert (x (d / "out" / "data.txt") == fs::perms::none);
#endif

  fs::remove_all (d);
}
