#include <launcher/steam/steam-depot.hxx>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/beast/core/error.hpp>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <system_error>

#include <launcher/launcher-log.hxx>
#include <launcher/steam/steam-compression.hxx>

using namespace std;
using namespace std::chrono;

namespace launcher
{
  namespace
  {
    constexpr auto info ([] (auto&&... a)
    {
      log::info (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr auto warning ([] (auto&&... a)
    {
      log::warning (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr auto trace_l2 ([] (auto&&... a)
    {
      log::trace_l2 (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr auto trace_l3 ([] (auto&&... a)
    {
      log::trace_l3 (categories::steam (), forward<decltype (a)> (a)...);
    });

    constexpr unsigned manifest_format_version = 5;

    constexpr size_t read_buffer_size (1u << 20);
  }

  string steam_depot_target::
  to_string () const
  {
    return "app " + std::to_string (app_id) + " depot " +
           std::to_string (depot_id) + " manifest " +
           std::to_string (manifest_id);
  }

  steam_depot_installer::
  steam_depot_installer (asio::io_context& i,
                         steam_content_client& c,
                         http_client_traits ht,
                         steam_depot_traits t)
    : ioc_ (i),
      content_ (c),
      http_traits_ (move (ht)),
      traits_ (move (t))
  {
    if (traits_.parallel_chunks == 0)
      traits_.parallel_chunks = 1;
    else if (traits_.parallel_chunks > 32)
      traits_.parallel_chunks = 32;

    if (traits_.chunk_attempts == 0)
      traits_.chunk_attempts = 1;

    http_traits_.request_timeout = traits_.request_timeout;

    http_ = make_unique<http_client> (ioc_, http_traits_);
  }

  const steam_content_server& steam_depot_installer::
  next_server ()
  {
    assert (!servers_.empty () && "no content servers have been discovered");

    const steam_content_server& s (servers_[server_cursor_]);

    server_cursor_ = (server_cursor_ + 1) % servers_.size ();

    return s;
  }

  asio::awaitable<steam_depot_manifest> steam_depot_installer::
  fetch_manifest (const steam_depot_target& t, uint32_t cell)
  {
    crypto::aes_key k (co_await content_.depot_key (t.app_id, t.depot_id));

    co_return co_await fetch_manifest_with_key (t, k, cell);
  }

  asio::awaitable<steam_depot_manifest> steam_depot_installer::
  fetch_manifest_with_key (const steam_depot_target& t,
                           const crypto::aes_key& k,
                           uint32_t cell)
  {
    if (!t.valid ())
      throw invalid_argument ("the depot target is incomplete: " +
                              t.to_string ());

    uint64_t code (co_await content_.manifest_request_code (
      t.app_id, t.depot_id, t.manifest_id, t.branch));

    servers_ = co_await content_.fetch_servers (
      cell, t.app_id, traits_.max_servers);

    server_cursor_ = 0;

    cdn_token_ = co_await content_.cdn_auth_token (
      t.app_id, t.depot_id, servers_.front ().host);

    string last;

    for (size_t a (0); a != min (servers_.size (), traits_.chunk_attempts);
         ++a)
    {
      const steam_content_server& s (next_server ());

      string url (s.base_url () + "/depot/" +
                  std::to_string (t.depot_id) + "/manifest/" +
                  std::to_string (t.manifest_id) + "/" +
                  std::to_string (manifest_format_version) + "/" +
                  std::to_string (code));

      if (!cdn_token_.empty ())
        url += cdn_token_.front () == '?' ? cdn_token_ : "?" + cdn_token_;

      trace_l2 ("requesting manifest from {}", s.host);

      try
      {
        http_response r (co_await http_->get (url));

        if (r.status_code () != 200)
          throw runtime_error ("HTTP " + std::to_string (r.status_code ()));

        if (!r.has_body () || r.body->empty ())
          throw runtime_error ("empty response");

        steam_depot_manifest m (parse_depot_manifest_archive (
          pb::byte_view (
            reinterpret_cast<const uint8_t*> (r.body->data ()),
            r.body->size ()),
          &k));

        if (m.depot_id != t.depot_id || m.manifest_id != t.manifest_id)
          throw runtime_error (
            "server returned depot " + std::to_string (m.depot_id) +
            " manifest " + std::to_string (m.manifest_id));

        co_return m;
      }
      catch (const exception& e)
      {
        last = e.what ();

        warning ("failed to fetch the manifest from {}: {}", s.host, last);
      }
    }

    throw runtime_error ("unable to download the manifest for " +
                         t.to_string () + "; last error: " +
                         (last.empty () ? "none reported" : last));
  }

  bool steam_depot_installer::
  already_installed (const steam_depot_file& f, const fs::path& p) const
  {
    error_code ec;

    if (!fs::exists (p, ec) || ec)
      return false;

    uintmax_t n (fs::file_size (p, ec));

    if (ec || n != f.size)
      return false;

    if (!traits_.verify_existing)
      return true;

    ifstream i (p, ios::binary);

    if (!i)
      return false;

    try
    {
      crypto::sha1_hasher h;

      vector<char> b (read_buffer_size);

      for (;;)
      {
        i.read (b.data (), static_cast<streamsize> (b.size ()));

        streamsize m (i.gcount ());

        if (m > 0)
          h.update (pb::byte_view (
            reinterpret_cast<const uint8_t*> (b.data ()),
            static_cast<size_t> (m)));

        if (!i)
          break;
      }

      if (!i.eof ())
        return false;

      return h.finish () == f.content_sha;
    }
    catch (const crypto::crypto_error& e)
    {
      warning ("could not verify {}: {}", p.string (), e.what ());

      return false;
    }
  }

  steam_depot_installer::plan steam_depot_installer::
  build_plan (const steam_depot_manifest& m, const fs::path& root) const
  {
    plan r;

    r.files.reserve (m.files.size ());

    for (const steam_depot_file& f : m.files)
    {
      file_plan p;
      p.file = &f;

      if (f.directory () || f.symlink ())
      {
        p.download = false;
        r.files.push_back (p);

        continue;
      }

      if (!traits_.force_download &&
          already_installed (f, root / fs::path (f.path)))
      {
        p.download        = false;
        r.bytes_present  += f.size;
      }
      else
      {
        p.download            = true;
        r.bytes_to_download  += f.size;
        r.chunks_to_fetch    += f.chunks.size ();
      }

      r.files.push_back (p);
    }

    return r;
  }

  asio::awaitable<crypto::byte_buffer> steam_depot_installer::
  fetch_chunk (const steam_depot_chunk& c,
               uint32_t depot,
               const crypto::aes_key& k)
  {
    string n (c.name ());
    string last;

    for (size_t a (0); a != traits_.chunk_attempts; ++a)
    {
      const steam_content_server& s (next_server ());

      string url (s.base_url () + "/depot/" + std::to_string (depot) +
                  "/chunk/" + n);

      if (!cdn_token_.empty ())
        url += cdn_token_.front () == '?' ? cdn_token_ : "?" + cdn_token_;

      try
      {
        http_response r (co_await http_->get (url));

        if (r.status_code () != 200)
          throw runtime_error ("HTTP " + std::to_string (r.status_code ()));

        if (!r.has_body () || r.body->empty ())
          throw runtime_error ("empty response");

        pb::byte_view b (
          reinterpret_cast<const uint8_t*> (r.body->data ()),
          r.body->size ());

        crypto::byte_buffer d (crypto::symmetric_decrypt (k, b));

        crypto::byte_buffer p (compression::decompress_depot_chunk (
          pb::byte_view (d.data (), d.size ()), c.uncompressed_size));

        uint32_t sum (compression::adler32_steam (
          pb::byte_view (p.data (), p.size ())));

        if (sum != c.checksum)
          throw runtime_error (
            "checksum mismatch (computed " + std::to_string (sum) +
            ", manifest declares " + std::to_string (c.checksum) + ")");

        co_return p;
      }
      catch (const exception& e)
      {
        last = e.what ();

        trace_l3 ("chunk {} failed on {}: {}", n, s.host, last);
      }
    }

    throw runtime_error ("failed to download chunk " + n + " of depot " +
                         std::to_string (depot) + " after " +
                         std::to_string (traits_.chunk_attempts) +
                         " attempts; last error: " + last);
  }

  asio::awaitable<uint64_t> steam_depot_installer::
  install_file (const steam_depot_file& f,
                const fs::path& p,
                uint32_t depot,
                const crypto::aes_key& k,
                steam_depot_result& res,
                const function<void (uint64_t)>& on_bytes)
  {
    error_code ec;

    fs::create_directories (p.parent_path (), ec);

    if (ec)
      throw system_error (ec,
                          "failed to create " +
                            p.parent_path ().string ());

    fs::path t (p);
    t += ".part";

    {
      fstream o (t, ios::binary | ios::out | ios::trunc);

      if (!o)
        throw runtime_error ("failed to open " + t.string () +
                             " for writing");

      o.close ();

      fs::resize_file (t, f.size, ec);

      if (ec)
        throw system_error (ec,
                            "failed to reserve " + std::to_string (f.size) +
                              " bytes for " + t.string ());

      o.open (t, ios::binary | ios::in | ios::out);

      if (!o)
        throw runtime_error ("failed to reopen " + t.string ());

      const size_t workers (min (traits_.parallel_chunks,
                                 max<size_t> (f.chunks.size (), 1)));

      size_t             next (0);
      size_t             active (workers);
      exception_ptr      failure;
      asio::steady_timer done (ioc_);

      done.expires_at (asio::steady_timer::time_point::max ());

      auto worker ([&] () -> asio::awaitable<void>
      {
        for (;;)
        {
          if (failure)
            co_return;

          size_t i (next++);

          if (i >= f.chunks.size ())
            co_return;

          const steam_depot_chunk& c (f.chunks[i]);

          try
          {
            crypto::byte_buffer d (co_await fetch_chunk (c, depot, k));

            assert (d.size () == c.uncompressed_size);

            o.seekp (static_cast<streamoff> (c.offset), ios::beg);

            if (!o)
              throw runtime_error ("failed to seek within " + t.string ());

            o.write (reinterpret_cast<const char*> (d.data ()),
                     static_cast<streamsize> (d.size ()));

            if (!o)
              throw runtime_error ("failed to write to " + t.string ());

            ++res.chunks_fetched;
            res.bytes_downloaded += c.compressed_size;
            res.bytes_written    += d.size ();

            if (on_bytes)
              on_bytes (d.size ());
          }
          catch (...)
          {
            if (!failure)
              failure = current_exception ();

            co_return;
          }
        }
      });

      for (size_t i (0); i != workers; ++i)
      {
        asio::co_spawn (ioc_, worker (), [&] (exception_ptr ep)
        {
          if (ep && !failure)
            failure = ep;

          if (--active == 0)
            done.cancel ();
        });
      }

      if (active != 0)
      {
        beast::error_code ec2;

        co_await done.async_wait (
          asio::redirect_error (asio::use_awaitable, ec2));
      }

      if (failure)
      {
        error_code ig;
        o.close ();
        fs::remove (t, ig);

        rethrow_exception (failure);
      }

      o.flush ();

      if (!o)
        throw runtime_error ("failed to flush " + t.string ());
    }

    {
      uintmax_t n (fs::file_size (t, ec));

      if (ec || n != f.size)
      {
        fs::remove (t, ec);

        throw runtime_error ("assembled " + p.filename ().string () +
                             " is " + std::to_string (ec ? 0 : n) +
                             " bytes, but the manifest declares " +
                             std::to_string (f.size));
      }
    }

    fs::rename (t, p, ec);

    if (ec)
    {
      fs::remove (p, ec);
      fs::rename (t, p, ec);

      if (ec)
      {
        error_code ig;
        fs::remove (t, ig);

        throw system_error (ec,
                            "failed to move " + t.string () + " into place");
      }
    }

    if (f.executable ())
    {
      fs::permissions (p,
                       fs::perms::owner_exec | fs::perms::group_exec |
                         fs::perms::others_exec,
                       fs::perm_options::add,
                       ec);
    }

    co_return f.size;
  }

  asio::awaitable<steam_depot_result> steam_depot_installer::
  install (const steam_depot_target& t,
           const fs::path& root,
           progress_coordinator* pc,
           uint32_t cell)
  {
    steady_clock::time_point started (steady_clock::now ());

    steam_depot_result res;

    crypto::aes_key k (co_await content_.depot_key (t.app_id, t.depot_id));

    steam_depot_manifest m (co_await fetch_manifest_with_key (t, k, cell));

    info ("depot {} manifest {} describes {} files totalling {} bytes",
          m.depot_id,
          m.manifest_id,
          m.file_count (),
          m.payload_size ());

    error_code ec;
    fs::create_directories (root, ec);

    if (ec)
      throw system_error (ec, "failed to create " + root.string ());

    plan p (build_plan (m, root));

    res.bytes_skipped = p.bytes_present;

    info ("{} bytes already present, {} bytes to download across {} chunks",
          p.bytes_present,
          p.bytes_to_download,
          p.chunks_to_fetch);

    for (const file_plan& f : p.files)
    {
      if (!f.file->directory ())
        continue;

      fs::create_directories (root / fs::path (f.file->path), ec);

      if (ec)
        throw system_error (ec,
                            "failed to create " +
                              (root / fs::path (f.file->path)).string ());
    }

    if (p.bytes_to_download == 0)
      info ("every file in depot {} is already present and correct",
            m.depot_id);

    shared_ptr<progress_entry> pe;

    if (pc != nullptr && p.bytes_to_download != 0)
    {
      if (!pc->running ())
        pc->start ();

      pe = pc->add_entry ("Downloading depot " + std::to_string (m.depot_id));
    }

    uint64_t done (0);

    auto on_bytes ([&pc, &pe, &done, &p] (uint64_t n)
    {
      done += n;

      if (pc != nullptr && pe)
        pc->update_progress (pe, done, p.bytes_to_download);
    });

    for (const file_plan& f : p.files)
    {
      const steam_depot_file& d (*f.file);

      if (d.directory ())
        continue;

      fs::path fp (root / fs::path (d.path));

      if (d.symlink ())
      {
        fs::remove (fp, ec);

        fs::create_directories (fp.parent_path (), ec);
        fs::create_symlink (fs::path (d.link_target), fp, ec);

        if (ec)
          warning ("could not create the symbolic link {} -> {}: {}",
                   d.path,
                   d.link_target,
                   ec.message ());

        continue;
      }

      if (!f.download)
      {
        ++res.files_skipped;
        continue;
      }

      trace_l2 ("installing {} ({} bytes, {} chunks)",
                d.path,
                d.size,
                d.chunks.size ());

      co_await install_file (d, fp, m.depot_id, k, res, on_bytes);

      ++res.files_written;
    }

    if (pc != nullptr && pe)
      pc->remove_entry (pe);

    res.elapsed = duration_cast<milliseconds> (steady_clock::now () -
                                               started);

    info ("installed {} files ({} skipped) from depot {} in {} ms",
          res.files_written,
          res.files_skipped,
          m.depot_id,
          res.elapsed.count ());

    co_return res;
  }
}
