namespace hello
{
  template <typename H, typename T>
  inline std::vector<std::shared_ptr<typename basic_download_manager<H, T>::task_type>>
  basic_download_manager<H, T>::
  sort_by_priority () const
  {
    std::vector<std::shared_ptr<task_type>> sorted (tasks_);

    std::sort (sorted.begin (),
               sorted.end (),
               [] (const auto& a, const auto& b)
    {
      // Higher priority first.
      //
      return a->request.priority > b->request.priority;
    });

    return sorted;
  }
}
