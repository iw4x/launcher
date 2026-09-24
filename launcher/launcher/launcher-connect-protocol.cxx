#include <launcher/launcher-connect-protocol.hxx>

#ifdef _WIN32
#  include <string>

#  include <windows.h>

#  include <launcher/launcher-log.hxx>
#endif

using namespace std;

namespace launcher
{
#ifdef _WIN32
  namespace
  {
    bool
    set_registry_string (const wchar_t* key_path,
                         const wchar_t* value_name,
                         const wstring& value)
    {
      HKEY key (nullptr);

      LONG r (RegCreateKeyExW (HKEY_CURRENT_USER,
                               key_path,
                               0,
                               nullptr,
                               REG_OPTION_NON_VOLATILE,
                               KEY_SET_VALUE,
                               nullptr,
                               &key,
                               nullptr));

      if (r != ERROR_SUCCESS)
        return false;

      DWORD size (static_cast<DWORD> ((value.size () + 1) *
                                      sizeof (wchar_t)));

      r = RegSetValueExW (key,
                          value_name,
                          0,
                          REG_SZ,
                          reinterpret_cast<const BYTE*> (value.c_str ()),
                          size);

      RegCloseKey (key);
      return r == ERROR_SUCCESS;
    }
  }
#endif

  void
  refresh_connect_protocol (const filesystem::path& executable) noexcept
  {
#ifdef _WIN32
    const wstring quoted (L"\"" + executable.native () + L"\"");

    bool r (
      set_registry_string (L"SOFTWARE\\Classes\\iw4x",
                           nullptr,
                           L"URL:IW4x Protocol") &&
      set_registry_string (L"SOFTWARE\\Classes\\iw4x",
                           L"URL Protocol",
                           L"") &&
      set_registry_string (L"SOFTWARE\\Classes\\iw4x\\DefaultIcon",
                           nullptr,
                           quoted + L",1") &&
      set_registry_string (L"SOFTWARE\\Classes\\iw4x\\shell\\open\\command",
                           nullptr,
                           quoted + L" \"%1\""));

    if (r)
      log::trace_l2 (categories::launcher (),
                     "refreshed iw4x URL protocol association: {}",
                     executable.string ());
    else
      log::warning (categories::launcher (),
                    "failed to refresh iw4x URL protocol association");
#else
    (void) executable;
#endif
  }
}
