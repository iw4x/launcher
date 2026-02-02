namespace launcher
{
  template <typename T>
  bool basic_cache_database<T>::
  open () const noexcept
  {
    return db_ != nullptr;
  }

  template <typename T>
  const fs::path& basic_cache_database<T>::
  path () const noexcept
  {
    return path_;
  }

  template <typename T>
  typename basic_cache_database<T>::database_type&
  basic_cache_database<T>::
  db () noexcept
  {
    return *db_;
  }

  template <typename T>
  const typename basic_cache_database<T>::database_type&
  basic_cache_database<T>::
  db () const noexcept
  {
    return *db_;
  }

  template <typename T>
  std::optional<cached_file> basic_cache_database<T>::
  find (const fs::path& p) const
  {
    return find (p.string ());
  }
}
