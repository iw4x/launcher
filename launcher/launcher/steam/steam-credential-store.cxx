#include <launcher/steam/steam-credential-store.hxx>

#include <boost/json.hpp>

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#  include <windows.h>
#  include <dpapi.h>
#else
#  include <dlfcn.h>
#  include <unistd.h>

#  include <boost/filesystem/path.hpp>
#  include <boost/process.hpp>
#endif

#include <launcher/launcher-log.hxx>
#include <launcher/steam/steam-crypto.hxx>

using namespace std;

namespace launcher
{
  namespace
  {
    namespace json = boost::json;

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

    constexpr const char* keyring_schema    = "io.iw4x.launcher.SteamToken";
    constexpr const char* keyring_attr_app  = "application";
    constexpr const char* keyring_attr_acct = "account";
    constexpr const char* keyring_app_value = "iw4x-launcher";

    constexpr int credential_format_version = 1;

    constexpr const char* file_aad = "iw4x-launcher/steam-credentials/v1";

    string
    sanitize (const string& s)
    {
      string r;
      r.reserve (s.size ());

      for (unsigned char c : s)
      {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_')
          r += static_cast<char> (c);
        else if (c >= 'A' && c <= 'Z')
          r += static_cast<char> (c - 'A' + 'a');
        else
          r += '_';
      }

      return r.empty () ? string ("default") : r;
    }

#ifndef _WIN32

    class libsecret
    {
    public:
      enum attribute_type
      {
        attribute_string = 0
      };

      enum schema_flags
      {
        schema_none = 0
      };

      using schema = void;

      static libsecret&
      instance ()
      {
        static libsecret l;
        return l;
      }

      bool
      available () const noexcept
      {
        return handle_ != nullptr && schema_ != nullptr;
      }

      const string&
      why () const noexcept
      {
        return why_;
      }

      bool
      store (const string& account, const string& secret, string& error) const
      {
        assert (available ());

        GError* e (nullptr);

        string label ("iw4x-launcher: Steam session for " + account);

        int r (store_ (schema_,
                       "default",
                       label.c_str (),
                       secret.c_str (),
                       nullptr,
                       &e,
                       keyring_attr_app,
                       keyring_app_value,
                       keyring_attr_acct,
                       account.c_str (),
                       nullptr));

        if (r == 0)
        {
          error = take_error (e);
          return false;
        }

        return true;
      }

      bool
      lookup (const string& account,
              string& secret,
              bool& found,
              string& error) const
      {
        assert (available ());

        GError* e (nullptr);

        char* p (lookup_ (schema_,
                          nullptr,
                          &e,
                          keyring_attr_app,
                          keyring_app_value,
                          keyring_attr_acct,
                          account.c_str (),
                          nullptr));

        if (e != nullptr)
        {
          error = take_error (e);
          found = false;

          return false;
        }

        found = p != nullptr;

        if (p != nullptr)
        {
          secret = p;

          free_ (p);
        }

        return true;
      }

      bool
      clear (const string& account, string& error) const
      {
        assert (available ());

        GError* e (nullptr);

        clear_ (schema_,
                nullptr,
                &e,
                keyring_attr_app,
                keyring_app_value,
                keyring_attr_acct,
                account.c_str (),
                nullptr);

        if (e != nullptr)
        {
          error = take_error (e);
          return false;
        }

        return true;
      }

    private:
      struct GError
      {
        unsigned int domain;
        int          code;
        char*        message;
      };

      using schema_new_t   = schema* (*) (const char*, int, ...);
      using store_t        = int (*) (const schema*,
                                      const char*,
                                      const char*,
                                      const char*,
                                      void*,
                                      GError**,
                                      ...);
      using lookup_t       = char* (*) (const schema*, void*, GError**, ...);
      using clear_t        = int (*) (const schema*, void*, GError**, ...);
      using free_t         = void (*) (char*);
      using error_free_t   = void (*) (GError*);

      libsecret ()
      {
        for (const char* n : {"libsecret-1.so.0", "libsecret-1.so"})
        {
          handle_ = dlopen (n, RTLD_LAZY | RTLD_LOCAL);

          if (handle_ != nullptr)
            break;
        }

        if (handle_ == nullptr)
        {
          why_ = "libsecret is not installed";
          return;
        }

        auto bind ([this] (auto& p, const char* n) -> bool
        {
          p = reinterpret_cast<remove_reference_t<decltype (p)>> (
            dlsym (handle_, n));

          if (p == nullptr)
            why_ = string ("libsecret is missing the symbol ") + n;

          return p != nullptr;
        });

        if (!bind (schema_new_, "secret_schema_new") ||
            !bind (store_, "secret_password_store_sync") ||
            !bind (lookup_, "secret_password_lookup_sync") ||
            !bind (clear_, "secret_password_clear_sync") ||
            !bind (free_, "secret_password_free") ||
            !bind (error_free_, "g_error_free"))
        {
          dlclose (handle_);
          handle_ = nullptr;

          return;
        }

        schema_ = schema_new_ (keyring_schema,
                               schema_none,
                               keyring_attr_app,
                               attribute_string,
                               keyring_attr_acct,
                               attribute_string,
                               nullptr);

        if (schema_ == nullptr)
        {
          why_ = "libsecret refused to register our schema";

          dlclose (handle_);
          handle_ = nullptr;
        }
      }

      ~libsecret () = default;

      libsecret (const libsecret&) = delete;
      libsecret& operator= (const libsecret&) = delete;

      string
      take_error (GError* e) const
      {
        if (e == nullptr)
          return "unknown error";

        string m (e->message != nullptr ? e->message : "unknown error");

        error_free_ (e);

        return m;
      }

    private:
      void*   handle_ = nullptr;
      schema* schema_ = nullptr;
      string  why_;

      schema_new_t   schema_new_   = nullptr;
      store_t        store_        = nullptr;
      lookup_t       lookup_       = nullptr;
      clear_t        clear_        = nullptr;
      free_t         free_         = nullptr;
      error_free_t   error_free_   = nullptr;
    };

    int
    run_secret_tool (const vector<string>& args,
                     const string& input,
                     string& output)
    {
      namespace bp = boost::process;

      output.clear ();

      try
      {
        boost::filesystem::path exe (bp::search_path ("secret-tool"));

        if (exe.empty ())
          return -1;

        bp::opstream in;
        bp::ipstream out;

        bp::child c (exe,
                     bp::args (args),
                     bp::std_in < in,
                     bp::std_out > out,
                     bp::std_err > bp::null);

        if (!input.empty ())
          in << input;

        in.flush ();
        in.pipe ().close ();

        output.assign (istreambuf_iterator<char> (out),
                       istreambuf_iterator<char> ());

        c.wait ();

        return c.exit_code ();
      }
      catch (const exception& e)
      {
        trace_l2 ("secret-tool could not be run: {}", e.what ());
        return -1;
      }
    }
#endif
  }

  string
  to_string (credential_backend b)
  {
    switch (b)
    {
    case credential_backend::none:           return "none";
    case credential_backend::dpapi:          return "Windows DPAPI";
    case credential_backend::libsecret:      return "Secret Service (libsecret)";
    case credential_backend::secret_tool:    return "Secret Service (secret-tool)";
    case credential_backend::encrypted_file: return "machine-bound encrypted file";
    }

    return "unknown";
  }

  bool
  keyring_backed (credential_backend b) noexcept
  {
    return b == credential_backend::dpapi ||
           b == credential_backend::libsecret ||
           b == credential_backend::secret_tool;
  }

  steam_credential_store::
  steam_credential_store (fs::path d)
    : directory_ (move (d))
  {
    select_backend ();

    if (keyring_backed (backend_))
      trace_l2 ("storing Steam credentials in the {}", to_string (backend_));
    else if (backend_ == credential_backend::encrypted_file)
      warning ("no system keyring is available; Steam credentials will be "
               "kept in a machine-bound encrypted file under {}",
               directory_.string ());
    else
      warning ("no credential store is available; you will have to sign in "
               "to Steam on every run");

    for (const string& n : notes_)
      trace_l2 ("credential store probe: {}", n);
  }

  void steam_credential_store::
  select_backend ()
  {
#ifdef _WIN32

    backend_ = credential_backend::dpapi;
#else

    const char* bus (getenv ("DBUS_SESSION_BUS_ADDRESS"));

    if (bus == nullptr || *bus == '\0')
    {
      notes_.push_back ("no session D-Bus is available, so no Secret "
                        "Service can be reached");
    }
    else
    {
      const libsecret& l (libsecret::instance ());

      if (l.available ())
      {
        string s, e;
        bool   found (false);

        if (l.lookup ("\x01probe", s, found, e))
        {
          backend_ = credential_backend::libsecret;
          return;
        }

        notes_.push_back ("libsecret loaded but the Secret Service did not "
                          "answer: " + e);
      }
      else
        notes_.push_back (l.why ());

      string o;

      if (run_secret_tool ({"lookup", keyring_attr_app, "\x01probe"},
                           string (),
                           o) >= 0)
      {
        backend_ = credential_backend::secret_tool;
        return;
      }

      notes_.push_back ("the secret-tool command is not usable either");
    }

    backend_ = credential_backend::encrypted_file;
#endif
  }

  string steam_credential_store::
  serialize (const steam_stored_credentials& c)
  {
    json::object o;

    o["version"]       = credential_format_version;
    o["account_name"]  = c.account_name;
    o["steam_id"]      = std::to_string (c.steam_id);
    o["refresh_token"] = c.refresh_token;
    o["guard_data"]    = c.guard_data;

    return json::serialize (o);
  }

  optional<steam_stored_credentials> steam_credential_store::
  deserialize (const string& s)
  {
    try
    {
      json::value v (json::parse (s));
      const json::object& o (v.as_object ());

      auto str ([&o] (const char* k) -> string
      {
        auto i (o.find (k));

        return i != o.end () && i->value ().is_string ()
                 ? string (i->value ().as_string ())
                 : string ();
      });

      auto i (o.find ("version"));

      if (i == o.end () || !i->value ().is_int64 () ||
          i->value ().as_int64 () != credential_format_version)
        return nullopt;

      steam_stored_credentials c;

      c.account_name  = str ("account_name");
      c.refresh_token = str ("refresh_token");
      c.guard_data    = str ("guard_data");

      string id (str ("steam_id"));

      if (!id.empty ())
      {
        size_t n (0);
        c.steam_id = stoull (id, &n);

        if (n != id.size ())
          return nullopt;
      }

      if (c.refresh_token.empty () || c.steam_id == 0)
        return nullopt;

      return c;
    }
    catch (const exception&)
    {
      return nullopt;
    }
  }

  fs::path steam_credential_store::
  secret_path (const string& a) const
  {
    return directory_ / ("steam-" + sanitize (a) + ".cred");
  }

  vector<uint8_t> steam_credential_store::
  machine_binding () const
  {
    string s ("iw4x-launcher-credential-binding");

#ifdef _WIN32

    wchar_t b[MAX_COMPUTERNAME_LENGTH + 1] {};
    DWORD   n (MAX_COMPUTERNAME_LENGTH + 1);

    if (GetComputerNameW (b, &n))
      s.append (reinterpret_cast<const char*> (b), n * sizeof (wchar_t));
#else

    for (const char* p : {"/etc/machine-id", "/var/lib/dbus/machine-id"})
    {
      ifstream f (p);

      if (f)
      {
        string l;
        getline (f, l);

        if (!l.empty ())
        {
          s += ':';
          s += l;

          break;
        }
      }
    }

    s += ':';
    s += std::to_string (static_cast<unsigned long> (getuid ()));

    if (const char* h = getenv ("HOME"); h != nullptr)
    {
      s += ':';
      s += h;
    }
#endif

    return vector<uint8_t> (s.begin (), s.end ());
  }

  optional<string> steam_credential_store::
  read_secret (const string& a) const
  {
    switch (backend_)
    {
    case credential_backend::none:
      return nullopt;

#ifdef _WIN32
    case credential_backend::dpapi:
      {
        fs::path p (secret_path (a));

        error_code ec;

        if (!fs::exists (p, ec))
          return nullopt;

        ifstream f (p, ios::binary);

        if (!f)
          return nullopt;

        string blob ((istreambuf_iterator<char> (f)),
                     istreambuf_iterator<char> ());

        DATA_BLOB in {static_cast<DWORD> (blob.size ()),
                      reinterpret_cast<BYTE*> (blob.data ())};

        string   ent (file_aad);
        DATA_BLOB entropy {static_cast<DWORD> (ent.size ()),
                           reinterpret_cast<BYTE*> (ent.data ())};

        DATA_BLOB out {};

        if (!CryptUnprotectData (
              &in, nullptr, &entropy, nullptr, nullptr, 0, &out))
        {
          warning ("could not decrypt the stored Steam credentials; you "
                   "will be asked to sign in again");
          return nullopt;
        }

        string r (reinterpret_cast<const char*> (out.pbData), out.cbData);

        SecureZeroMemory (out.pbData, out.cbData);
        LocalFree (out.pbData);

        return r;
      }
#else
    case credential_backend::dpapi:

      assert (false && "DPAPI backend selected off Windows");
      return nullopt;

    case credential_backend::libsecret:
      {
        string s, e;
        bool   found (false);

        if (!libsecret::instance ().lookup (a, s, found, e))
        {
          warning ("could not read the Steam credentials from the keyring: "
                   "{}",
                   e);
          return nullopt;
        }

        if (!found)
          return nullopt;

        return s;
      }

    case credential_backend::secret_tool:
      {
        string o;

        int r (run_secret_tool ({"lookup",
                                 keyring_attr_app,
                                 keyring_app_value,
                                 keyring_attr_acct,
                                 a},
                                string (),
                                o));

        if (r != 0 || o.empty ())
          return nullopt;

        while (!o.empty () && (o.back () == '\n' || o.back () == '\r'))
          o.pop_back ();

        return o;
      }
#endif

    case credential_backend::encrypted_file:
      {
        fs::path p (secret_path (a));

        error_code ec;

        if (!fs::exists (p, ec))
          return nullopt;

        ifstream f (p, ios::binary);

        if (!f)
          return nullopt;

        string blob ((istreambuf_iterator<char> (f)),
                     istreambuf_iterator<char> ());

        try
        {
          vector<uint8_t> b (machine_binding ());
          string          salt (keyring_schema);

          crypto::aes_key k (crypto::hkdf_sha256 (
            crypto::byte_view (b.data (), b.size ()),
            crypto::byte_view (
              reinterpret_cast<const uint8_t*> (salt.data ()), salt.size ()),
            file_aad));

          string aad (file_aad);

          crypto::byte_buffer d (crypto::aead_decrypt (
            k,
            crypto::byte_view (
              reinterpret_cast<const uint8_t*> (blob.data ()), blob.size ()),
            crypto::byte_view (
              reinterpret_cast<const uint8_t*> (aad.data ()), aad.size ())));

          return string (d.begin (), d.end ());
        }
        catch (const crypto::crypto_error& e)
        {
          warning ("could not decrypt {}: {}", p.string (), e.what ());
          return nullopt;
        }
      }
    }

    return nullopt;
  }

  void steam_credential_store::
  write_secret (const string& a, const string& s)
  {
    switch (backend_)
    {
    case credential_backend::none:
      return;

#ifdef _WIN32
    case credential_backend::dpapi:
      {
        string    ent (file_aad);
        DATA_BLOB entropy {static_cast<DWORD> (ent.size ()),
                           reinterpret_cast<BYTE*> (ent.data ())};

        string    in_copy (s);
        DATA_BLOB in {static_cast<DWORD> (in_copy.size ()),
                      reinterpret_cast<BYTE*> (in_copy.data ())};

        DATA_BLOB out {};

        if (!CryptProtectData (&in,
                               L"iw4x-launcher Steam session",
                               &entropy,
                               nullptr,
                               nullptr,
                               CRYPTPROTECT_UI_FORBIDDEN,
                               &out))
          throw system_error (static_cast<int> (GetLastError ()),
                              system_category (),
                              "failed to protect the Steam credentials");

        error_code ec;
        fs::create_directories (directory_, ec);

        fs::path p (secret_path (a));

        {
          ofstream f (p, ios::binary | ios::trunc);

          if (!f)
          {
            LocalFree (out.pbData);

            throw runtime_error ("failed to open " + p.string () +
                                 " for writing");
          }

          f.write (reinterpret_cast<const char*> (out.pbData), out.cbData);
          f.flush ();

          if (!f)
          {
            LocalFree (out.pbData);

            throw runtime_error ("failed to write " + p.string ());
          }
        }

        LocalFree (out.pbData);

        return;
      }
#else
    case credential_backend::dpapi:
      assert (false && "DPAPI backend selected off Windows");
      return;

    case credential_backend::libsecret:
      {
        string e;

        if (!libsecret::instance ().store (a, s, e))
          throw runtime_error (
            "failed to store the Steam credentials in the keyring: " + e);

        return;
      }

    case credential_backend::secret_tool:
      {
        string o;

        int r (run_secret_tool ({"store",
                                 "--label=iw4x-launcher: Steam session for " + a,
                                 keyring_attr_app,
                                 keyring_app_value,
                                 keyring_attr_acct,
                                 a},
                                s,
                                o));

        if (r != 0)
          throw runtime_error (
            "secret-tool failed to store the Steam credentials (exit " +
            std::to_string (r) + ")");

        return;
      }
#endif

    case credential_backend::encrypted_file:
      {
        error_code ec;
        fs::create_directories (directory_, ec);

        if (ec)
          throw system_error (ec,
                              "failed to create " + directory_.string ());

        vector<uint8_t> b (machine_binding ());
        string          salt (keyring_schema);
        string          aad (file_aad);

        crypto::aes_key k (crypto::hkdf_sha256 (
          crypto::byte_view (b.data (), b.size ()),
          crypto::byte_view (
            reinterpret_cast<const uint8_t*> (salt.data ()), salt.size ()),
          file_aad));

        crypto::byte_buffer d (crypto::aead_encrypt (
          k,
          crypto::byte_view (
            reinterpret_cast<const uint8_t*> (s.data ()), s.size ()),
          crypto::byte_view (
            reinterpret_cast<const uint8_t*> (aad.data ()), aad.size ())));

        fs::path p (secret_path (a));

        {
          ofstream f (p, ios::binary | ios::trunc);

          if (!f)
            throw runtime_error ("failed to open " + p.string () +
                                 " for writing");

          f.write (reinterpret_cast<const char*> (d.data ()),
                   static_cast<streamsize> (d.size ()));
          f.flush ();

          if (!f)
            throw runtime_error ("failed to write " + p.string ());
        }

#ifndef _WIN32

        fs::permissions (p,
                         fs::perms::owner_read | fs::perms::owner_write,
                         fs::perm_options::replace,
                         ec);
#endif

        return;
      }
    }
  }

  void steam_credential_store::
  clear_secret (const string& a)
  {
    switch (backend_)
    {
    case credential_backend::none:
      return;

#ifndef _WIN32
    case credential_backend::libsecret:
      {
        string e;

        if (!libsecret::instance ().clear (a, e))
          warning ("failed to remove the Steam credentials from the "
                   "keyring: {}",
                   e);

        return;
      }

    case credential_backend::secret_tool:
      {
        string o;

        run_secret_tool ({"clear",
                          keyring_attr_app,
                          keyring_app_value,
                          keyring_attr_acct,
                          a},
                         string (),
                         o);

        return;
      }
#endif

    case credential_backend::dpapi:
    case credential_backend::encrypted_file:
      {
        error_code ec;
        fs::remove (secret_path (a), ec);

        return;
      }

    default:
      return;
    }
  }

  optional<steam_stored_credentials> steam_credential_store::
  load (const string& a) const
  {
    if (a.empty ())
      return nullopt;

    optional<string> s (read_secret (a));

    if (!s)
      return nullopt;

    optional<steam_stored_credentials> c (deserialize (*s));

    crypto::secure_zero (*s);

    if (!c)
    {
      warning ("the stored Steam credentials for '{}' could not be read and "
               "will be ignored",
               a);
      return nullopt;
    }

    if (!c->account_name.empty () && c->account_name != a)
    {
      warning ("the stored Steam credentials for '{}' name account '{}'; "
               "ignoring them",
               a,
               c->account_name);
      return nullopt;
    }

    trace_l2 ("loaded stored Steam credentials for '{}'", a);

    return c;
  }

  void steam_credential_store::
  store (const steam_stored_credentials& c)
  {
    if (c.account_name.empty ())
      throw invalid_argument ("cannot store credentials without an account "
                              "name");

    if (c.empty ())
      throw invalid_argument ("cannot store credentials without a refresh "
                              "token");

    if (backend_ == credential_backend::none)
      return;

    string s (serialize (c));

    try
    {
      write_secret (c.account_name, s);
    }
    catch (...)
    {
      crypto::secure_zero (s);
      throw;
    }

    crypto::secure_zero (s);

    trace_l2 ("stored Steam credentials for '{}' in the {}",
              c.account_name,
              to_string (backend_));
  }

  void steam_credential_store::
  erase (const string& a)
  {
    if (a.empty ())
      return;

    clear_secret (a);
  }
}
