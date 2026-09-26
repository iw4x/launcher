# Boost.Asio configuration parser backport

The `boost/asio/impl/config.hpp` header is copied from Boost 1.88.0 under the accompanying Boost Software License.

Note that the launcher currently pins Boost 1.87.0, whose Asio configuration parser has two bugs that can surface during startup. The parser does not clear `errno` before converting integer configuration values, so an unrelated earlier error can leave `errno` set and make an otherwise valid setting fail with `config out of range`. We can encounter this when creating the hashing thread pool during startup and again later when initializing Asio I/O services.

The same implementation sizes its integer conversion buffers using `max_digits10`. For integer types this value is zero, which can reduce concurrency hints to empty strings before they are parsed.

Boost 1.88.0 fixes both cases by clearing `errno` before each conversion and sizing the integer buffers using `digits10`. Until the launcher can move to that version, we provide the fixed header locally and place this include directory before the Boost dependency headers. This ordering must apply to every launcher and test translation unit so that all Asio users see the same implementation.

Remove this override once the pinned Boost packages have been updated to 1.88.0 or later. Keep the regression tests at that point since they cover the original failure independently of this workaround.

Sources:

- https://github.com/chriskohlhoff/asio/issues/1588
- https://github.com/boostorg/asio/blob/boost-1.88.0/include/boost/asio/impl/config.hpp
