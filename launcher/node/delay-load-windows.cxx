// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <delayimp.h>

#include <string.h> // _stricmp()

// The Node-API functions that we import name node.exe as their module (see
// buildfile). The process that loads us is Electron running under the name
// of the application, and it exports these functions from its own
// executable. Since node.exe is delay-loaded, the delay-load helper calls
// our hook before loading it, and we hand over the process executable
// instead.
//
namespace launcher
{
  namespace node
  {
    namespace
    {
      FARPROC WINAPI
      delay_load_hook (unsigned int n, PDelayLoadInfo i)
      {
        if (n == dliNotePreLoadLibrary && _stricmp (i->szDll, "node.exe") == 0)
          return reinterpret_cast<FARPROC> (GetModuleHandleW (nullptr));

        return nullptr;
      }
    }
  }
}

// The helper reads the hook from this variable. Its declaration differs
// between the Microsoft and the MinGW runtimes (the former is const), so we
// take the type from the declaration.
//
decltype (__pfnDliNotifyHook2) __pfnDliNotifyHook2 =
  &launcher::node::delay_load_hook;
