#include <iostream>

#include <hello/hello-options.hxx>
#include <hello/version.hxx>

int
main (int argc, char* argv[])
{
  using namespace std;
  using namespace hello;

  try
  {
    options opt (argc, argv);

    // Handle --build2-metadata (see also buildfile).
    //
    if (opt.build2_metadata_specified ())
    {
      auto& o (cout);

      // The export.metadata variable must be the first non-blank, non-comment
      // line.
      //
      o << "# build2 buildfile hello"                                  << "\n"
        << "export.metadata = 1 hello"                                 << "\n"
        << "hello.name = [string] hello"                               << "\n"
        << "hello.version = [string] '"  << HELLO_VERSION_FULL << '\'' << "\n"
        << "hello.checksum = [string] '" << HELLO_VERSION_FULL << '\'' << "\n";

      return 0;
    }

    // Handle --version.
    //
    if (opt.version ())
    {
      auto& o (cout);

      o << "Hello " << HELLO_VERSION_ID << "\n";

      return 0;
    }

    // Handle --help.
    //
    if (opt.help ())
    {
      auto& o (cout);

      o << "usage: hello [options] <names>" << "\n"
        << "options:"                       << "\n";

      opt.print_usage (o);

      return 0;
    }
  }

  catch (const cli::exception& ex)
  {
    auto& e (cerr);

    e << "error: " << ex.what () << "\n";

    return 1;
  }
}
