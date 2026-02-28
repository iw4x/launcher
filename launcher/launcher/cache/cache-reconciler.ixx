namespace launcher
{
  template <typename T>
  typename basic_reconciler<T>::db_type&
  basic_reconciler<T>::
  database () noexcept
  {
    launcher::log::trace_l3 (categories::cache{}, "accessing mutable database reference");
    return db_;
  }

  template <typename T>
  const typename basic_reconciler<T>::db_type&
  basic_reconciler<T>::
  database () const noexcept
  {
    launcher::log::trace_l3 (categories::cache{}, "accessing const database reference");
    return db_;
  }

  template <typename T>
  const fs::path& basic_reconciler<T>::
  root () const noexcept
  {
    launcher::log::trace_l3 (categories::cache{}, "accessing root path: {}", root_.string ());
    return root_;
  }
}
