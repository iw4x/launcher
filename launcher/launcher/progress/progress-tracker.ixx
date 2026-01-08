namespace launcher
{
  // Inline helper to get current time in microseconds.
  //
  inline std::uint64_t
  current_time_us () noexcept
  {
    using namespace std::chrono;
    auto now (steady_clock::now ());
    auto us (duration_cast<microseconds>(now.time_since_epoch ()));
    return static_cast<std::uint64_t>(us.count ());
  }
}
