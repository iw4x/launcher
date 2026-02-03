#include <launcher/launcher-download.hxx>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>

#include <launcher/download/download-manager.hxx>
#include <launcher/download/download-task.hxx>

using namespace std;

namespace launcher
{
  // Note: we default to serial execution (1) unless told otherwise.
  //
  download_coordinator::
  download_coordinator (asio::io_context& c)
    : download_coordinator (c, 1)
  {
  }

  download_coordinator::
  download_coordinator (asio::io_context& c, size_t n)
    : ioc_ (c),
      manager_ (make_unique<manager_type> (c, n)),
      http_ (make_unique<http_client> (c))
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
  queue_download (string u, fs::path f)
  {
    // Synthesize the request.
    //
    // Note that we infer the display name from the filename so the UI has
    // something reasonable to show without the caller having to be explicit.
    //
    request_type r;
    r.urls.push_back (move (u));
    r.name = f.filename ().string ();
    r.target = move (f);

    return queue_download (move (r));
  }

  // Statistics & State.
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
    // Block here until the manager drains the queue.
    //
    co_await manager_->download_all ();

    // @@ TODO: We might want to aggregate errors into a composite exception
    // later if we find that checking individual tasks is too cumbersome.
    //
    co_return;
  }

  void download_coordinator::
  clear ()
  {
    manager_->clear ();
  }

  // Accessors.
  //

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
