#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace launcher
{
  namespace fs = std::filesystem;

  enum class credential_backend
  {
    none,

    dpapi,

    libsecret,

    secret_tool,

    encrypted_file
  };

  std::string
  to_string (credential_backend);

  bool
  keyring_backed (credential_backend) noexcept;

  struct steam_stored_credentials
  {
    std::string   account_name;
    std::uint64_t steam_id = 0;

    std::string refresh_token;

    std::string guard_data;

    bool
    empty () const noexcept
    {
      return refresh_token.empty ();
    }
  };

  class steam_credential_store
  {
  public:
    explicit
    steam_credential_store (fs::path directory);

    steam_credential_store (const steam_credential_store&) = delete;
    steam_credential_store& operator= (const steam_credential_store&) = delete;

    credential_backend
    backend () const noexcept
    {
      return backend_;
    }

    const std::vector<std::string>&
    probe_notes () const noexcept
    {
      return notes_;
    }

    std::optional<steam_stored_credentials>
    load (const std::string& account_name) const;

    void
    store (const steam_stored_credentials&);

    void
    erase (const std::string& account_name);

  private:
    void
    select_backend ();

    static std::string
    serialize (const steam_stored_credentials&);

    static std::optional<steam_stored_credentials>
    deserialize (const std::string&);

    std::optional<std::string>
    read_secret (const std::string& account_name) const;

    void
    write_secret (const std::string& account_name, const std::string&);

    void
    clear_secret (const std::string& account_name);

    fs::path
    secret_path (const std::string& account_name) const;

    std::vector<std::uint8_t>
    machine_binding () const;

  private:
    fs::path                 directory_;
    credential_backend       backend_ = credential_backend::none;
    std::vector<std::string> notes_;
  };
}
