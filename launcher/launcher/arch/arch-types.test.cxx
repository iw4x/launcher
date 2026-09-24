#include <launcher/arch/arch-types.hxx>

#include <cassert>
#include <sstream>
#include <string>

using namespace std;
using namespace launcher;

static void
check (const string& s, architecture e)
{
  auto a (parse_architecture (s));

  assert (a.has_value ());
  assert (*a == e);
}

static void
check_fail (const string& s)
{
  assert (!parse_architecture (s).has_value ());
}

int
main ()
{
  assert (to_string (architecture::x86) == "x86");
  assert (to_string (architecture::x64) == "x64");

  for (architecture a : architectures)
    check (string (to_string (a)), a);

  check ("X86", architecture::x86);
  check ("X64", architecture::x64);
  check ("X64", architecture::x64);

  check ("32", architecture::x86);
  check ("i386", architecture::x86);
  check ("i686", architecture::x86);
  check ("win32", architecture::x86);

  check ("64", architecture::x64);
  check ("amd64", architecture::x64);
  check ("x86_64", architecture::x64);
  check ("x86-64", architecture::x64);
  check ("win64", architecture::x64);

  check_fail ("");
  check_fail ("x");
  check_fail ("x8");
  check_fail ("x866");
  check_fail ("x86 ");
  check_fail ("arm64");
  check_fail ("86");

  for (architecture a : architectures)
  {
    assert (!architecture_label (a).empty ());
    assert (!architecture_description (a).empty ());
    assert (!architecture_executable (a).empty ());
  }

  assert (architecture_executable (architecture::x86) == "iw4x.exe");
  assert (architecture_executable (architecture::x64) == "iw4x (mm).exe");

  assert (architecture_label (architecture::x86) !=
          architecture_label (architecture::x64));

  for (architecture a : architectures)
  {
    ostringstream o;
    o << a;

    assert (o.str () == to_string (a));
  }

  return 0;
}
