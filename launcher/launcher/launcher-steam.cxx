#include <launcher/launcher-steam.hxx>

#include <boost/asio/use_awaitable.hpp>

#include <cassert>
#include <utility>

#include <launcher/cache/cache-database.hxx>
#include <launcher/launcher-log.hxx>
#include <launcher/steam/steam-crypto.hxx>
#include <launcher/steam/steam-prompt.hxx>

using namespace std;

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

    constexpr const char* setting_last_account = "steam_last_account";

    string
    setting_installed_manifest (uint32_t depot)
    {
      return "steam_depot_" + std::to_string (depot) + "_manifest";
    }

    constexpr uint32_t default_app_id = 10180;

    struct default_depot
    {
      uint32_t depot_id;
      uint64_t manifest_id;
    };

    constexpr default_depot default_depots[] =
    {
      {10182, 4063260329988186194ull},
      {10183, 8707803059832053468ull}
    };

    constexpr chrono::milliseconds license_wait (5000);
  }

  vector<steam_depot_target>
  steam_default_targets ()
  {
    vector<steam_depot_target> r;

    r.reserve (size (default_depots));

    for (const default_depot& d: default_depots)
    {
      steam_depot_target t;

      t.app_id      = default_app_id;
      t.depot_id    = d.depot_id;
      t.manifest_id = d.manifest_id;
      t.branch      = "public";

      r.push_back (move (t));
    }

    return r;
  }

  steam_coordinator::
  steam_coordinator (asio::io_context& i, http_client_traits ht, fs::path c)
    : ioc_ (i),
      http_traits_ (move (ht)),
      cache_directory_ (move (c)),
      credentials_ (cache_directory_)
  {
  }

  string steam_coordinator::
  last_account () const
  {
    try
    {
      cache_database db (cache_directory_);

      return db.setting_value (setting_last_account);
    }
    catch (const exception& e)
    {
      trace_l2 ("could not read the last used Steam account: {}", e.what ());

      return string ();
    }
  }

  void steam_coordinator::
  remember_account (const string& a)
  {
    if (a.empty ())
      return;

    try
    {
      cache_database db (cache_directory_);

      db.setting (setting_last_account, a);
    }
    catch (const exception& e)
    {
      trace_l2 ("could not record the last used Steam account: {}",
                e.what ());
    }
  }

  uint64_t steam_coordinator::
  installed_manifest (uint32_t depot) const
  {
    try
    {
      cache_database db (cache_directory_);

      string v (db.setting_value (setting_installed_manifest (depot)));

      if (v.empty ())
        return 0;

      size_t   n (0);
      uint64_t r (stoull (v, &n));

      return n == v.size () ? r : 0;
    }
    catch (const exception& e)
    {
      trace_l2 ("could not read the installed manifest for depot {}: {}",
                depot,
                e.what ());

      return 0;
    }
  }

  void steam_coordinator::
  record_installed_manifest (uint32_t depot, uint64_t manifest)
  {
    try
    {
      cache_database db (cache_directory_);

      db.setting (setting_installed_manifest (depot),
                  std::to_string (manifest));
    }
    catch (const exception& e)
    {
      trace_l2 ("could not record the installed manifest for depot {}: {}",
                depot,
                e.what ());
    }
  }

  string steam_coordinator::
  preferred_account (const steam_install_options& o) const
  {
    if (!o.account_name.empty ())
      return o.account_name;

    if (o.force_login)
      return string ();

    return last_account ();
  }

  void steam_coordinator::
  forget (const string& a)
  {
    string n (a.empty () ? last_account () : a);

    if (n.empty ())
    {
      info ("no remembered Steam session to forget");
      return;
    }

    credentials_.erase (n);

    try
    {
      cache_database db (cache_directory_);

      db.erase_setting (setting_last_account);
    }
    catch (const exception& e)
    {
      trace_l2 ("could not clear the last used Steam account: {}", e.what ());
    }

    info ("forgot the remembered Steam session for '{}'", n);
  }

  asio::awaitable<steam_auth_tokens> steam_coordinator::
  interactive_login (steam_cm_client& cm, const steam_install_options& o)
  {
    if (!prompt::interactive ())
      throw runtime_error (
        "signing in to Steam needs an interactive terminal, but standard "
        "input is not one; run the launcher from a terminal, or sign in once "
        "there so the session can be remembered");

    steam_authenticator a (ioc_, cm);
    steam_auth_prompt   p (prompt::console_prompt ());

    steam_auth_tokens t;

    if (o.use_qr)
    {
      t = co_await a.log_in_with_qr (p);
    }
    else
    {
      string account (o.account_name);

      if (account.empty ())
        account = prompt::read_line ("Steam account name: ");

      if (account.empty ())
        throw runtime_error ("no Steam account name was given");

      string password (prompt::read_secret ("Steam password: "));

      if (password.empty ())
        throw runtime_error ("no Steam password was given");

      string guard;

      if (auto c = credentials_.load (account); c)
        guard = c->guard_data;

      try
      {
        t = co_await a.log_in_with_credentials (account, password, guard, p);
      }
      catch (...)
      {
        crypto::secure_zero (password);
        throw;
      }

      crypto::secure_zero (password);
    }

    if (o.remember)
    {
      steam_stored_credentials c;

      c.account_name  = t.account_name;
      c.steam_id      = t.steam_id;
      c.refresh_token = t.refresh_token;
      c.guard_data    = t.guard_data;

      try
      {
        credentials_.store (c);
        remember_account (c.account_name);

        info ("remembered this Steam session in the {}",
              to_string (credentials_.backend ()));
      }
      catch (const exception& e)
      {
        warning ("could not remember the Steam session: {}", e.what ());
      }
    }

    co_return t;
  }

  asio::awaitable<void> steam_coordinator::
  authenticate (steam_cm_client& cm, const steam_install_options& o)
  {
    string account (preferred_account (o));

    if (!o.force_login && !account.empty ())
    {
      if (auto c = credentials_.load (account); c && !c->empty ())
      {
        trace_l2 ("reusing the remembered Steam session for '{}'", account);

        string stale;

        try
        {
          co_await cm.log_on (c->steam_id, c->refresh_token, o.cell_id);
        }
        catch (const exception& e)
        {
          stale = e.what ();
        }

        if (stale.empty ())
        {
          remember_account (account);

          info ("signed in to Steam as '{}' using the remembered session",
                account);

          co_return;
        }

        info ("the remembered Steam session for '{}' is no longer valid "
              "({}); signing in again",
              account,
              stale);

        credentials_.erase (account);

        co_await cm.disconnect ();
        co_await cm.connect_any (o.cell_id);
      }
    }

    steam_install_options io (o);

    if (io.account_name.empty ())
      io.account_name = account;

    steam_auth_tokens t (co_await interactive_login (cm, io));

    co_await cm.log_on (t.steam_id, t.refresh_token, o.cell_id);

    info ("signed in to Steam as '{}'", t.account_name);
  }

  asio::awaitable<steam_depot_result> steam_coordinator::
  install (const steam_install_options& o,
           const fs::path& root,
           progress_coordinator* pc)
  {
    if (o.targets.empty ())
      throw invalid_argument ("no Steam depot was given to install");

    for (const steam_depot_target& t: o.targets)
    {
      if (!t.valid ())
        throw invalid_argument ("the Steam depot to install is not fully "
                                "specified: " + t.to_string ());
    }

    steam_cm_traits ct;

    ct.verify_ssl = http_traits_.verify_ssl;
    ct.proxy_url  = http_traits_.proxy_url;

    steam_cm_client cm (ioc_, ct);

    exception_ptr        failure;
    steam_depot_result   result;

    try
    {
      co_await cm.connect_any (o.cell_id);
      co_await authenticate (cm, o);

      if (!co_await cm.await_licenses (license_wait))
        trace_l2 ("Steam did not send a license list; continuing anyway");
      else
        trace_l2 ("the account holds {} licenses", cm.licenses ().size ());

      steam_content_client content (cm);

      for (size_t i (0); i != o.targets.size (); ++i)
      {
        const steam_depot_target& t (o.targets[i]);

        steam_depot_traits dt (o.depot);

        dt.force_download = o.force_download;

        const bool installed (installed_manifest (t.depot_id) ==
                              t.manifest_id);

        if (installed && dt.verify_existing && !dt.force_download)
        {
          trace_l2 ("depot {} was last installed at manifest {}; verifying by "
                    "size only",
                    t.depot_id,
                    t.manifest_id);

          dt.verify_existing = false;
        }

        steam_depot_installer installer (ioc_, content, http_traits_, dt);

        info ("installing {} ({} of {}) into {}",
              t.to_string (),
              i + 1,
              o.targets.size (),
              root.string ());

        steam_depot_result r (
          co_await installer.install (t, root, pc, o.cell_id));

        record_installed_manifest (t.depot_id, t.manifest_id);

        result.bytes_downloaded += r.bytes_downloaded;
        result.bytes_written    += r.bytes_written;
        result.bytes_skipped    += r.bytes_skipped;
        result.files_written    += r.files_written;
        result.files_skipped    += r.files_skipped;
        result.chunks_fetched   += r.chunks_fetched;
        result.elapsed          += r.elapsed;
      }
    }
    catch (...)
    {
      failure = current_exception ();
    }

    try
    {
      co_await cm.disconnect ();
    }
    catch (const exception& e)
    {
      trace_l2 ("error while disconnecting from Steam: {}", e.what ());
    }

    if (failure)
      rethrow_exception (failure);

    co_return result;
  }
}
