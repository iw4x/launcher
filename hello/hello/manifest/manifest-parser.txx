#include <boost/asio/experimental/awaitable_operators.hpp>

namespace hello
{
  using namespace asio::experimental::awaitable_operators;

  template <typename F, typename T>
  asio::awaitable<typename basic_manifest_parser<F, T>::manifest_type>
  basic_manifest_parser<F, T>::
  parse (const string_type& json_str, manifest_format kind)
  {
    co_return co_await manifest_type::parse_async (json_str, kind);
  }

  template <typename F, typename T>
  asio::awaitable<std::vector<typename basic_manifest_parser<F, T>::manifest_type>>
  basic_manifest_parser<F, T>::
  parse_parallel (const std::vector<string_type>& json_strings,
                 manifest_format kind)
  {
    std::vector<manifest_type> r;
    r.reserve (json_strings.size ());

    for (const auto& json_str : json_strings)
    {
      auto m (co_await parse (json_str, kind));
      r.push_back (std::move (m));
    }

    co_return r;
  }

  template <typename F, typename T>
  asio::awaitable<bool> basic_manifest_parser<F, T>::
  validate (const manifest_type& manifest)
  {
    co_return co_await manifest.validate_async ();
  }

  template <typename F, typename T>
  asio::awaitable<std::vector<bool>> basic_manifest_parser<F, T>::
  validate_parallel (const std::vector<manifest_type>& manifests)
  {
    std::vector<bool> r;
    r.reserve (manifests.size ());

    for (const auto& m : manifests)
    {
      bool v (co_await validate (m));
      r.push_back (v);
    }

    co_return r;
  }

  // Standalone functions.
  //
  inline asio::awaitable<manifest>
  parse_update_manifest (const std::string& json_str)
  {
    co_return co_await manifest_parser::parse (json_str, manifest_format::update);
  }

  inline asio::awaitable<manifest>
  parse_dlc_manifest (const std::string& json_str)
  {
    co_return co_await manifest_parser::parse (json_str, manifest_format::dlc);
  }

  inline asio::awaitable<bool>
  validate_manifest (const manifest& m)
  {
    co_return co_await manifest_parser::validate (m);
  }
}
