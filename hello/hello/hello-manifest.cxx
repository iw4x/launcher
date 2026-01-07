#include <hello/hello-manifest.hxx>

#include <hello/manifest/manifest.hxx>

#include <boost/json.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace hello
{
  namespace json = boost::json;

  manifest_coordinator::manifest_type manifest_coordinator::
  parse (const std::string& json_str,
         manifest_format kind)
  {
    if (json_str.empty ())
      throw std::runtime_error ("manifest JSON is empty");

    manifest_type m (json_str, kind);

    m.link_files ();

    return m;
  }

  manifest_coordinator::manifest_type manifest_coordinator::
  load (const fs::path& file,
        manifest_format kind)
  {
    if (!fs::exists (file))
      throw std::runtime_error ("manifest file does not exist: " + file.string ());

    std::ifstream stream (file);
    if (!stream)
      throw std::runtime_error ("failed to open manifest file: " + file.string ());

    std::string json_str ((std::istreambuf_iterator<char> (stream)),
                          std::istreambuf_iterator<char> ());

    stream.close ();

    return parse (json_str, kind);
  }

  void manifest_coordinator::
  save (const manifest_type& m,
        const fs::path& file)
  {
    std::string json_str (m.string ());

    if (file.has_parent_path ())
    {
      std::error_code ec;
      fs::create_directories (file.parent_path (), ec);

      if (ec)
        throw std::runtime_error ("failed to create manifest directory: " +
                                  ec.message ());
    }

    std::ofstream stream (file);
    if (!stream)
      throw std::runtime_error ("failed to create manifest file: " +
                                file.string ());

    stream << json_str;

    if (!stream)
      throw std::runtime_error ("failed to write manifest file: " +
                                file.string ());
  }

  bool manifest_coordinator::
  validate (const manifest_type& m)
  {
    return m.validate ();
  }

  std::vector<manifest_coordinator::file_type> manifest_coordinator::
  get_missing_files (const manifest_type& m,
                     const fs::path& install_dir,
                     bool verify_hashes)
  {
    std::vector<file_type> missing;

    for (const auto& file : m.files)
    {
      if (file.archive_name)
        continue;

      if (!verify_file (file, install_dir, verify_hashes))
        missing.push_back (file);
    }

    return missing;
  }

  std::vector<manifest_coordinator::archive_type> manifest_coordinator::
  get_missing_archives (const manifest_type& m,
                        const fs::path& install_dir)
  {
    std::vector<archive_type> missing;

    for (const auto& archive : m.archives)
    {
      if (!verify_archive (archive, install_dir, false))
        missing.push_back (archive);
    }

    return missing;
  }

  bool manifest_coordinator::
  verify_file (const file_type& file,
               const fs::path& install_dir,
               bool verify_hash)
  {
    fs::path path (resolve_path (file, install_dir));

    std::error_code ec;
    if (!fs::exists (path, ec) || ec)
      return false;

    std::uintmax_t size (fs::file_size (path, ec));
    if (ec || size != file.size)
      return false;

    if (verify_hash && !file.hash.empty ())
    {
      std::string computed (compute_file_hash (path, file.hash.algorithm));

      if (!compare_hashes (computed, file.hash.value))
        return false;
    }

    return true;
  }

  bool manifest_coordinator::
  verify_archive (const archive_type& archive,
                  const fs::path& install_dir,
                  bool verify_hash)
  {
    fs::path path (resolve_path (archive, install_dir));

    std::error_code ec;
    if (!fs::exists (path, ec) || ec)
      return false;

    std::uintmax_t size (fs::file_size (path, ec));
    if (ec || size != archive.size)
      return false;

    if (verify_hash && !archive.hash.empty ())
    {
      std::string computed (compute_file_hash (path, archive.hash.algorithm));

      if (!compare_hashes (computed, archive.hash.value))
        return false;
    }

    return true;
  }

  fs::path manifest_coordinator::
  resolve_path (const file_type& file,
                const fs::path& install_dir)
  {
    return install_dir / file.path;
  }

  fs::path manifest_coordinator::
  resolve_path (const archive_type& archive,
                const fs::path& install_dir)
  {
    return install_dir / archive.name;
  }

  asio::awaitable<void> manifest_coordinator::
  extract_archive (const archive_type& archive,
                   const fs::path& archive_path,
                   const fs::path& install_dir)
  {
    throw std::runtime_error ("archive extraction not implemented for: " +
                              archive.name);

    co_return;
  }

  std::uint64_t manifest_coordinator::
  calculate_download_size (const manifest_type& m,
                           const fs::path& install_dir)
  {
    std::uint64_t total (0);

    std::vector<file_type> missing_files (
        get_missing_files (m, install_dir, false));

    for (const auto& file : missing_files)
      total += file.size;

    std::vector<archive_type> missing_archives (
      get_missing_archives (m, install_dir));

    for (const auto& archive : missing_archives)
      total += archive.size;

    return total;
  }

  std::size_t manifest_coordinator::
  get_file_count (const manifest_type& m)
  {
    std::size_t count (m.files.size ());

    for (const auto& archive : m.archives)
      count += archive.files.size ();

    return count;
  }

  bool manifest_coordinator::
  is_empty (const manifest_type& m)
  {
    return m.empty ();
  }

  std::string
  compute_hash (const void* data,
                std::size_t size,
                hash_algorithm algorithm)
  {
    if (algorithm != hash_algorithm::blake3)
      throw std::runtime_error ("unsupported hash algorithm");

    throw std::runtime_error ("blake3 hash computation not implemented");
  }

  std::string
  compute_file_hash (const fs::path& file,
                     hash_algorithm algorithm)
  {
    if (algorithm != hash_algorithm::blake3)
      throw std::runtime_error ("unsupported hash algorithm");

    throw std::runtime_error ("blake3 hash computation not implemented");
  }

  bool
  compare_hashes (const std::string& hash1,
                  const std::string& hash2)
  {
    if (hash1.size () != hash2.size ())
      return false;

    return std::equal (hash1.begin (),
                       hash1.end (),
                       hash2.begin (),
                       [] (char a, char b)
    {
      return std::tolower (static_cast<unsigned char> (a)) ==
        std::tolower (static_cast<unsigned char> (b));
    });
  }
}
