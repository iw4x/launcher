#include <launcher/cache/cache-database.hxx>

#include <stdexcept>

#include <launcher/cache/cache-types-odb.hxx>

namespace launcher
{
  // Explicit template instantiation.
  //
  template class basic_cache_database<cache_database_traits<>>;
}
