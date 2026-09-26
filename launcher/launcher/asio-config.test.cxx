#include <boost/asio/config.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>

#include <cassert>
#include <cerrno>
#include <chrono>
#include <limits>
#include <stdexcept>

namespace asio = boost::asio;

// A previous library call may leave ERANGE set even when it succeeds. Neither
// context construction nor later service initialization should depend on it.
//
static void
test_context ()
{
  errno = ERANGE;
  asio::io_context io;

  errno = ERANGE;
  asio::steady_timer timer (io, std::chrono::milliseconds (0));

  errno = ERANGE;
  asio::ip::tcp::resolver resolver (io);

  errno = ERANGE;
  asio::ip::tcp::socket socket (io);

  bool completed (false);
  timer.async_wait ([&completed] (const boost::system::error_code& ec)
  {
    assert (!ec);
    completed = true;
  });

  io.run ();
  assert (completed);
}

static void
test_pool ()
{
  for (unsigned int n : {1U, 2U})
  {
    errno = ERANGE;
    asio::thread_pool pool (n);

    errno = ERANGE;
    int hint (asio::config (pool).get ("scheduler", "concurrency_hint", -1));
    assert (hint == (n == 1 ? 1 : 0));

    bool completed (false);
    asio::post (pool, [&completed] {completed = true;});
    pool.join ();
    assert (completed);
  }
}

static void
test_concurrency_hint ()
{
  // Integer configuration buffers must have room for the value and its sign.
  //
  for (int n : {-1, 1, 12, (std::numeric_limits<int>::max) ()})
  {
    errno = ERANGE;
    asio::execution_context context (asio::config_from_concurrency_hint {n});
    errno = ERANGE;
    assert (asio::config (context).get ("scheduler", "concurrency_hint", 0) == n);
  }
}

template <typename T>
static void
check_value (asio::config& config, const char* key, T expected)
{
  errno = ERANGE;
  T value (config.get ("test", key, T {}));
  assert (value == expected);
}

template <typename T>
static void
check_out_of_range (asio::config& config, const char* key)
{
  errno = 0;
  try
  {
    (void) config.get ("test", key, T {});
    assert (false);
  }
  catch (const std::out_of_range&)
  {
  }
}

static void
test_values ()
{
  asio::execution_context context (asio::config_from_string {
    "test.signed=-42\n"
    "test.unsigned=42\n"
    "test.true=1\n"
    "test.false=0\n"
    "test.signed_min=-9223372036854775808\n"
    "test.signed_max=9223372036854775807\n"
    "test.unsigned_max=18446744073709551615\n"
    "test.signed_overflow=9223372036854775808\n"
    "test.signed_underflow=-9223372036854775809\n"
    "test.unsigned_overflow=18446744073709551616\n"
    "test.narrow_overflow=256\n"
    "test.invalid_bool=2\n"});
  asio::config config (context);

  check_value (config, "signed", -42);
  check_value (config, "unsigned", 42U);
  check_value (config, "true", true);
  check_value (config, "false", false);
  check_value (config, "signed_min", (std::numeric_limits<long long>::min) ());
  check_value (config, "signed_max", (std::numeric_limits<long long>::max) ());
  check_value (config, "unsigned_max",
               (std::numeric_limits<unsigned long long>::max) ());

  errno = ERANGE;
  assert (config.get ("test", "missing", 123) == 123);

  check_out_of_range<long long> (config, "signed_overflow");
  check_out_of_range<long long> (config, "signed_underflow");
  check_out_of_range<unsigned long long> (config, "unsigned_overflow");
  check_out_of_range<unsigned char> (config, "narrow_overflow");
  check_out_of_range<bool> (config, "invalid_bool");
}

int
main ()
{
  test_context ();
  test_pool ();
  test_concurrency_hint ();
  test_values ();
}
