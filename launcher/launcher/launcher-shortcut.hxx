#pragma once

#include <filesystem>

#include <launcher/arch/arch.hxx>
#include <launcher/shortcut/shortcut.hxx>

namespace launcher
{
  namespace fs = std::filesystem;

  class shortcut_coordinator
  {
  public:
    shortcut_coordinator (fs::path launcher, fs::path root);

    shortcut_coordinator (const shortcut_coordinator&) = delete;
    shortcut_coordinator& operator= (const shortcut_coordinator&) = delete;

    void
    refresh () noexcept;

    shortcut_spec
    spec (architecture) const;

  private:
    fs::path launcher_;
    fs::path root_;
  };
}
