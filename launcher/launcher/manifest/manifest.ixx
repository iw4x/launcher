namespace launcher
{
  template <typename S>
  inline typename basic_hash<S>::string_type basic_hash<S>::
  string () const
  {
    // @@: Currently stubbed.
    //
    return value;
  }

  template <typename F, typename T>
  inline void basic_manifest<F, T>::
  link_files ()
  {
    for (auto& file : files)
    {
      if (file.archive_name)
      {
        // Find the archive with matching name.
        //
        for (auto& archive : archives)
        {
          if (archive.name == *file.archive_name)
          {
            archive.files.push_back (file);
            break;
          }
        }
      }
    }
  }

  template <typename F, typename T>
  inline bool basic_manifest<F, T>::
  validate () const
  {
    // Validate all files.
    //
    for (const auto& file : files)
    {
      if (!traits_type::validate_file (file))
        return false;
    }

    // Validate all archives.
    //
    for (const auto& archive : archives)
    {
      if (!traits_type::validate_archive (archive))
        return false;
    }

    return true;
  }
}
