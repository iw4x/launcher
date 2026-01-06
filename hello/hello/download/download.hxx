#pragma once

#include <hello/download/download-types.hxx>
#include <hello/download/download-request.hxx>
#include <hello/download/download-response.hxx>
#include <hello/download/download-task.hxx>
#include <hello/download/download-manager.hxx>

namespace hello
{
  namespace download
  {
    using hello::download_state;
    using hello::download_priority;
    using hello::download_verification;
    using hello::download_progress;
    using hello::download_error;

    using hello::download_request;
    using hello::download_response;
    using hello::download_task;
    using hello::download_manager;

    using hello::make_download_task;
  }
}
