// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <delayimp.h>

#include <string.h> // _stricmp()

namespace launcher
{
  namespace node
  {
    namespace
    {
      // Return the process executable in place of node.exe and nullptr
      // (default handling) for everything else.
      //
      // The Node-API functions that we import name node.exe as their module
      // (see buildfile). The process that loads us is Electron under the
      // name of the application, which exports these functions from its own
      // executable.
      //
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

// Register the hook with the delay-load helper.
//
// Note that the declaration differs between the Microsoft and MinGW runtimes
// (the former is const), thus decltype.
//
decltype (__pfnDliNotifyHook2) __pfnDliNotifyHook2 =
  &launcher::node::delay_load_hook;
