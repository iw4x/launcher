namespace launcher
{
  template <typename T>
  bool basic_cache_database<T>::
  open () const noexcept
  {
    launcher::log::trace_l3 (categories::cache{}, "checking if cache database is open");
    return db_ != nullptr;
  }

  template <typename T>
  const fs::path& basic_cache_database<T>::
  path () const noexcept
  {
    launcher::log::trace_l3 (categories::cache{}, "retrieving cache database path: {}", path_.string ());
    return path_;
  }

  template <typename T>
  typename basic_cache_database<T>::database_type&
  basic_cache_database<T>::
  db () noexcept
  {
    launcher::log::trace_l3 (categories::cache{}, "accessing mutable raw database handle");
    return *db_;
  }

  template <typename T>
  const typename basic_cache_database<T>::database_type&
  basic_cache_database<T>::
  db () const noexcept
  {
    launcher::log::trace_l3 (categories::cache{}, "accessing const raw database handle");
    return *db_;
  }

  template <typename T>
  std::optional<cached_file> basic_cache_database<T>::
  find (const fs::path& p) const
  {
    launcher::log::trace_l3 (categories::cache{}, "finding file in database by fs::path: {}", p.string ());
    return find (p.string ());
  }
}
