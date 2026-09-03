#pragma once

#include <launcher/steam/steam-auth.hxx>

#include <boost/asio/awaitable.hpp>

#include <string>

namespace launcher
{
  namespace prompt
  {
    bool
    interactive () noexcept;

    std::string
    read_line (const std::string& message);

    std::string
    read_secret (const std::string& message);

    void
    render_qr (const std::string& text);

    steam_auth_prompt
    console_prompt ();
  }
}
