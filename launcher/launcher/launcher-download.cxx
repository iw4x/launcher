#include <launcher/launcher-download.hxx>

#include <launcher/download/download-manager.hxx>
#include <launcher/download/download-task.hxx>

#include <openssl/evp.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <vector>

using namespace std;

namespace launcher
{
  download_coordinator::
  download_coordinator (asio::io_context& ioc)
    : ioc_ (ioc),
      manager_ (make_unique<manager_type> (ioc, 4)),
      http_ (make_unique<http_client> (ioc))
  {
  }

  download_coordinator::
  download_coordinator (asio::io_context& ioc, size_t mp)
    : ioc_ (ioc),
      manager_ (make_unique<manager_type> (ioc, mp)),
      http_ (make_unique<http_client> (ioc))
  {
  }

  void download_coordinator::
  set_max_parallel (size_t n)
  {
    manager_->set_max_parallel (n);
  }

  size_t download_coordinator::
  max_parallel () const
  {
    return manager_->max_parallel ();
  }

  void download_coordinator::
  set_completion_callback (completion_callback cb)
  {
    manager_->set_task_completion_callback (move (cb));
  }

  void download_coordinator::
  set_batch_completion_callback (batch_completion_callback cb)
  {
    manager_->set_batch_completion_callback (move (cb));
  }

  // Queue management.
  //

  shared_ptr<download_coordinator::task_type> download_coordinator::
  queue_download (request_type r)
  {
    return manager_->add_task (move (r));
  }

  shared_ptr<download_coordinator::task_type> download_coordinator::
  queue_download (string url, fs::path target)
  {
    request_type r;
    r.urls.push_back (move (url));
    r.target = move (target);

    // Default the task name to the filename. This is usually what we want
    // for UI reporting unless the caller overrides it.
    //
    r.name = r.target.filename ().string ();

    return queue_download (move (r));
  }

  shared_ptr<download_coordinator::task_type> download_coordinator::
  queue_download (string url,
                  fs::path target,
                  download_verification method,
                  string value)
  {
    request_type r;
    r.urls.push_back (move (url));
    r.target = move (target);
    r.name = r.target.filename ().string ();

    r.verification_method = method;
    r.verification_value = move (value);

    return queue_download (move (r));
  }

  // Statistics & State.
  //
  // Most of these are just pass-throughs to the manager, but they allow us
  // to keep the manager implementation details hidden from the coordinator's
  // clients.
  //

  size_t download_coordinator::
  total_count () const
  {
    return manager_->total_count ();
  }

  size_t download_coordinator::
  completed_count () const
  {
    return manager_->completed_count ();
  }

  size_t download_coordinator::
  failed_count () const
  {
    return manager_->failed_count ();
  }

  size_t download_coordinator::
  active_count () const
  {
    return manager_->active_count ();
  }

  uint64_t download_coordinator::
  total_bytes () const
  {
    return manager_->total_bytes ();
  }

  uint64_t download_coordinator::
  downloaded_bytes () const
  {
    return manager_->downloaded_bytes ();
  }

  download_progress download_coordinator::
  overall_progress () const
  {
    return manager_->overall_progress ();
  }

  vector<shared_ptr<download_coordinator::task_type>> download_coordinator::
  tasks () const
  {
    return manager_->tasks ();
  }

  // Execution.
  //

  asio::awaitable<void> download_coordinator::
  execute_all ()
  {
    // Wait for the manager to drain the queue.
    //
    co_await manager_->download_all ();

    // @@ TODO: Should we collect the specific errors here and throw a
    // composite exception? For now, we assume the caller checks the
    // task states or the failed count.
    //

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
