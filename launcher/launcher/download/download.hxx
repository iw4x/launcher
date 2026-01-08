#pragma once

#include <launcher/download/download-types.hxx>
#include <launcher/download/download-request.hxx>
#include <launcher/download/download-response.hxx>
#include <launcher/download/download-task.hxx>
#include <launcher/download/download-manager.hxx>

namespace launcher
{
  namespace download
  {
    using launcher::download_state;
    using launcher::download_priority;
    using launcher::download_verification;
    using launcher::download_progress;
    using launcher::download_error;

    using launcher::download_request;
    using launcher::download_response;
    using launcher::download_task;
    using launcher::download_manager;

    using launcher::make_download_task;
  }
}
