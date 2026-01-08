#include <launcher/manifest/manifest.hxx>

namespace launcher
{
  // Explicit template instantiations for common types.
  //
  template class basic_hash<std::string>;
  template class basic_manifest_file<std::string>;
  template class basic_manifest_archive<std::string>;
  template class basic_manifest<manifest_format>;

  // Hash verification instantiation.
  //
  template bool basic_hash<std::string>::verify (const std::string&) const;
  template bool basic_hash<std::string>::verify (const std::vector<char>&) const;
}
