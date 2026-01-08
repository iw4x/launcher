namespace launcher
{
  // Return a snapshot of tasks sorted by priority.
  //
  // Note that this returns a new vector containing shared pointers to the
  // tasks. We do this to provide a stable snapshot to the caller, as the
  // internal `tasks_` list might be modified by the background download loop or
  // other threads.
  //
  template <typename H, typename T>
  inline std::vector<std::shared_ptr<typename basic_download_manager<H, T>::task_type>>
  basic_download_manager<H, T>::
  sort_by_priority () const
  {
    using task_ptr = std::shared_ptr<task_type>;

    std::vector<task_ptr> r (tasks_);

    // Sort descending.
    //
    // We assume that a higher integer value for priority corresponds to a
    // more urgent task (e.g., 10 runs before 1).
    //
    std::stable_sort (r.begin (), r.end (), [] (const auto& a, const auto& b)
    {
      return a->request.priority > b->request.priority;
    });

    return r;
  }
}
