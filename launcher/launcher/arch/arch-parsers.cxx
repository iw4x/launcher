#include <launcher/arch/arch-parsers.hxx>

#include <launcher/launcher-options.hxx>

using namespace std;

namespace launcher
{
  namespace cli
  {
    void parser<architecture>::
    parse (architecture& r, bool& xs, scanner& s)
    {
      const char* o (s.next ());

      if (!s.more ())
        throw missing_value (o);

      string v (s.next ());

      if (optional<architecture> a = parse_architecture (v))
        r = *a;
      else
        throw invalid_value (o, v);

      xs = true;
    }
  }
}
