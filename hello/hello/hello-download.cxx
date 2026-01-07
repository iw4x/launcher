#include <hello/hello-download.hxx>

#include <hello/download/download-manager.hxx>
#include <hello/download/download-task.hxx>

#include <openssl/evp.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <vector>

using namespace std;

namespace hello
{
  download_coordinator::
  download_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      manager_ (std::make_unique<manager_type> (ioc, 4)),
      http_ (std::make_unique<http_client> (ioc))
  {
  }

  download_coordinator::
  download_coordinator (asio::io_context& ioc, std::size_t max_parallel)
    : ioc_ (ioc),
      manager_ (std::make_unique<manager_type> (ioc, max_parallel)),
      http_ (std::make_unique<http_client> (ioc))
  {
  }

  void download_coordinator::
  set_max_parallel (std::size_t n)
  {
    manager_->set_max_parallel (n);
  }

  std::size_t download_coordinator::
  max_parallel () const
  {
    return manager_->max_parallel ();
  }

  void download_coordinator::
  set_completion_callback (completion_callback cb)
  {
    manager_->set_task_completion_callback (std::move (cb));
  }

  void download_coordinator::
  set_batch_completion_callback (batch_completion_callback cb)
  {
    manager_->set_batch_completion_callback (std::move (cb));
  }

  std::shared_ptr<download_coordinator::task_type> download_coordinator::
  queue_download (request_type req)
  {
    return manager_->add_task (std::move (req));
  }

  std::shared_ptr<download_coordinator::task_type> download_coordinator::
  queue_download (std::string url, fs::path target)
  {
    request_type req;
    req.urls.push_back (std::move (url));
    req.target = std::move (target);
    req.name = req.target.filename ().string ();

    return queue_download (std::move (req));
  }

  std::shared_ptr<download_coordinator::task_type> download_coordinator::
  queue_download (std::string url,
                  fs::path target,
                  download_verification verification_method,
                  std::string verification_value)
  {
    request_type req;
    req.urls.push_back (std::move (url));
    req.target = std::move (target);
    req.name = req.target.filename ().string ();
    req.verification_method = verification_method;
    req.verification_value = std::move (verification_value);

    return queue_download (std::move (req));
  }

  std::size_t download_coordinator::
  total_count () const
  {
    return manager_->total_count ();
  }

  std::size_t download_coordinator::
  completed_count () const
  {
    return manager_->completed_count ();
  }

  std::size_t download_coordinator::
  failed_count () const
  {
    return manager_->failed_count ();
  }

  std::size_t download_coordinator::
  active_count () const
  {
    return manager_->active_count ();
  }

  std::uint64_t download_coordinator::
  total_bytes () const
  {
    return manager_->total_bytes ();
  }

  std::uint64_t download_coordinator::
  downloaded_bytes () const
  {
    return manager_->downloaded_bytes ();
  }

  download_progress download_coordinator::
  overall_progress () const
  {
    return manager_->overall_progress ();
  }

  std::vector<std::shared_ptr<download_coordinator::task_type>>
  download_coordinator::
  tasks () const
  {
    return manager_->tasks ();
  }

  asio::awaitable<void> download_coordinator::
  execute_all ()
  {
    co_await manager_->download_all ();

    std::size_t failed (failed_count ());
    if (failed > 0)
    {
      // @@: Maybe we should log or collect failed tasks for diagnostics?
      //
    }

    co_return;
  }

  void download_coordinator::
  clear ()
  {
    manager_->clear ();
  }

  download_coordinator::manager_type& download_coordinator::
  manager () noexcept
  {
    return *manager_;
  }

  const download_coordinator::manager_type& download_coordinator::
  manager () const noexcept
  {
    return *manager_;
  }
}
