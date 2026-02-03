namespace launcher
{
  template <typename T>
  typename basic_reconciler<T>::db_type&
  basic_reconciler<T>::
  database () noexcept
  {
    return db_;
  }

  template <typename T>
  const typename basic_reconciler<T>::db_type&
  basic_reconciler<T>::
  database () const noexcept
  {
    return db_;
  }

  template <typename T>
  const fs::path& basic_reconciler<T>::
  root () const noexcept
  {
    return root_;
  }
}
