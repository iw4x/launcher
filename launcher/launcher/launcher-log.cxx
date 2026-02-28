#include <launcher/launcher-log.hxx>

#include <string>
#include <initializer_list>

using namespace std;
using namespace quill;

namespace launcher
{
  logger* active_logger (nullptr);

  namespace
  {
    Logger*
    register_category (string const&                      n,
                       initializer_list<shared_ptr<Sink>> s,
                       PatternFormatterOptions const&     f,
                       LogLevel                           t)
    {
      Logger* l (Frontend::create_or_get_logger (n, s, f));
      l->set_log_level (t);
      return l;
    }
  }

  logger::
  logger ()
  {
    Backend::start ({
      .enable_yield_when_idle               = true,
      .sleep_duration                       = 0ns,
      .wait_for_queues_to_empty_before_exit = false,
      .check_printable_char                 = {},
      .log_level_short_codes                =
      {
        "3", "2", "1", "D", "I", "N", "W", "E", "C", "B", "_"
      }
    });

    ConsoleSinkConfig c;
    c.set_colour_mode (ConsoleSinkConfig::ColourMode::Never);

    RotatingFileSinkConfig r;
    r.set_rotation_frequency_and_interval ('H', 1);
    r.set_max_backup_files (24);
    r.set_rotation_max_file_size (1'000'000'000);
    r.set_overwrite_rolled_files (true);
    r.set_filename_append_option (FilenameAppendOption::StartDateTime);

    auto fs (Frontend::create_or_get_sink<RotatingFileSink> ("launcher.log", r));

    PatternFormatterOptions pf (
      "%(time) [%(log_level_short_code)] %(logger:<16) %(caller_function:<32) "
      "%(short_source_location:<24) %(message)",
      "%H:%M:%S.%Qms",
      Timezone::LocalTime);

    using namespace categories;

    log::detail::logger<launcher> () =
      register_category (string (log::policy<launcher>::name),
                         {fs},
                         pf,
                         log::policy<launcher>::threshold);

    log::detail::logger<download> () =
      register_category (string (log::policy<download>::name),
                         {fs},
                         pf,
                         log::policy<download>::threshold);

    log::detail::logger<cache> () =
      register_category (string (log::policy<cache>::name),
                         {fs},
                         pf,
                         log::policy<cache>::threshold);

    log::detail::logger<github> () =
      register_category (string (log::policy<github>::name),
                         {fs},
                         pf,
                         log::policy<github>::threshold);

    log::detail::logger<http> () =
      register_category (string (log::policy<http>::name),
                         {fs},
                         pf,
                         log::policy<http>::threshold);

    log::detail::logger<manifest> () =
      register_category (string (log::policy<manifest>::name),
                         {fs},
                         pf,
                         log::policy<manifest>::threshold);

    log::detail::logger<progress> () =
      register_category (string (log::policy<progress>::name),
                         {fs},
                         pf,
                         log::policy<progress>::threshold);

    log::detail::logger<steam> () =
      register_category (string (log::policy<steam>::name),
                         {fs},
                         pf,
                         log::policy<steam>::threshold);

    log::detail::logger<update> () =
      register_category (string (log::policy<update>::name),
                         {fs},
                         pf,
                         log::policy<update>::threshold);

    // In development builds, we blow the doors wide open and allow all trace
    // statements through so internals are visible. The compile-time minimum
    // level already permits this in LAUNCHER_DEVELOP mode, so we just need to
    // drop the runtime threshold here so they actually hit the sinks.
    //
#if LAUNCHER_DEVELOP
    log::detail::logger<launcher> ()->set_log_level (LogLevel::TraceL3);
    log::detail::logger<cache>    ()->set_log_level (LogLevel::TraceL3);
    log::detail::logger<download> ()->set_log_level (LogLevel::TraceL3);
    log::detail::logger<github>   ()->set_log_level (LogLevel::TraceL3);
    log::detail::logger<http>     ()->set_log_level (LogLevel::TraceL3);
    log::detail::logger<manifest> ()->set_log_level (LogLevel::TraceL3);
    log::detail::logger<progress> ()->set_log_level (LogLevel::TraceL3);
    log::detail::logger<steam>    ()->set_log_level (LogLevel::TraceL3);
    log::detail::logger<update>   ()->set_log_level (LogLevel::TraceL3);
#endif
  }

  logger::
  ~logger ()
  {
    using namespace categories;

    log::detail::logger<launcher> () = nullptr;
    log::detail::logger<cache>    () = nullptr;
    log::detail::logger<download> () = nullptr;
    log::detail::logger<github>   () = nullptr;
    log::detail::logger<http>     () = nullptr;
    log::detail::logger<manifest> () = nullptr;
    log::detail::logger<progress> () = nullptr;
    log::detail::logger<steam>    () = nullptr;
    log::detail::logger<update>   () = nullptr;

    Backend::stop ();
  }
}
