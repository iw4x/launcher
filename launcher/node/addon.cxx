// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <iterator> // size()

#include <node_api.h>

#include <launcher/version.hxx>

namespace launcher
{
  namespace node
  {
    namespace
    {
      // Throw a JavaScript Error with the specified message, unless the
      // Node-API call that failed has already left an exception pending.
      //
      // Note that only some failures leave one pending (e.g.,
      // napi_invalid_arg doesn't) and returning nullptr without it would
      // make the call quietly evaluate to undefined.
      //
      void
      throw_error (napi_env e, const char* m)
      {
        bool p;
        if (napi_is_exception_pending (e, &p) == napi_ok && !p)
          napi_throw_error (e, nullptr, m);
      }

      // Return the launcher version as a JavaScript string.
      //
      napi_value
      version (napi_env e, napi_callback_info)
      {
        napi_value r;
        if (napi_create_string_utf8 (e,
                                     LAUNCHER_VERSION_ID,
                                     NAPI_AUTO_LENGTH,
                                     &r) != napi_ok)
        {
          throw_error (e, "unable to create version string");
          return nullptr;
        }

        return r;
      }
    }
  }
}

// Note that Node-API calls this once for each JavaScript environment that
// loads the addon (the main thread and every worker that requires it), so
// any state we add has to be kept per environment (see
// napi_set_instance_data()).
//
NAPI_MODULE_INIT ()
{
  using namespace launcher;

  const napi_property_descriptor ds[]
  {
    {"version", nullptr, &node::version, nullptr, nullptr, nullptr,
     napi_default, nullptr}
  };

  if (napi_define_properties (env, exports, std::size (ds), ds) != napi_ok)
  {
    node::throw_error (env, "unable to define addon exports");
    return nullptr;
  }

  return exports;
}
