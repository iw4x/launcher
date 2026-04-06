#pragma once

#include <cassert>
#include <compare>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace launcher
{
  // HTTP method (verb).
  //
  enum class http_method
  {
    get,
    head,
    post,
    put,
    delete_,
    connect,
    options,
    trace,
    patch
  };

  std::string
  to_string (http_method);

  http_method
  to_http_method (const std::string&);

  std::ostream&
  operator << (std::ostream& o, http_method m);

  // HTTP status code.
  //
  enum class http_status : std::uint16_t
  {
    // Informational (1xx)
    continue_                       = 100,
    switching_protocols             = 101,
    processing                      = 102,
    early_hints                     = 103,

    // Success (2xx)
    ok                              = 200,
    created                         = 201,
    accepted                        = 202,
    non_authoritative_information   = 203,
    no_content                      = 204,
    reset_content                   = 205,
    partial_content                 = 206,
    multi_status                    = 207,
    already_reported                = 208,
    im_used                         = 226,

    // Redirection (3xx)
    multiple_choices                = 300,
    moved_permanently               = 301,
    found                           = 302,
    see_other                       = 303,
    not_modified                    = 304,
    use_proxy                       = 305,
    temporary_redirect              = 307,
    permanent_redirect              = 308,

    // Client error (4xx)
    bad_request                     = 400,
    unauthorized                    = 401,
    payment_required                = 402,
    forbidden                       = 403,
    not_found                       = 404,
    method_not_allowed              = 405,
    not_acceptable                  = 406,
    proxy_authentication_required   = 407,
    request_timeout                 = 408,
    conflict                        = 409,
    gone                            = 410,
    length_required                 = 411,
    precondition_failed             = 412,
    payload_too_large               = 413,
    uri_too_long                    = 414,
    unsupported_media_type          = 415,
    range_not_satisfiable           = 416,
    expectation_failed              = 417,
    im_a_teapot                     = 418,
    misdirected_request             = 421,
    unprocessable_entity            = 422,
    locked                          = 423,
    failed_dependency               = 424,
    too_early                       = 425,
    upgrade_required                = 426,
    precondition_required           = 428,
    too_many_requests               = 429,
    request_header_fields_too_large = 431,
    unavailable_for_legal_reasons   = 451,

    // Server error (5xx)
    internal_server_error           = 500,
    not_implemented                 = 501,
    bad_gateway                     = 502,
    service_unavailable             = 503,
    gateway_timeout                 = 504,
    http_version_not_supported      = 505,
    variant_also_negotiates         = 506,
    insufficient_storage            = 507,
    loop_detected                   = 508,
    not_extended                    = 510,
    network_authentication_required = 511
  };

  std::string
  to_string (http_status);

  std::ostream&
  operator << (std::ostream& o, http_status s);

  // HTTP header field.
  //
  struct http_field
  {
    std::string name;
    std::string value;

    http_field () = default;
    http_field (std::string n, std::string v);

    bool
    empty () const noexcept
    {
      return name.empty () && value.empty ();
    }

    bool
    operator== (const http_field&) const = default;
  };

  // HTTP headers collection.
  //
  struct http_headers
  {
    using fields_type = std::vector<http_field>;
    fields_type fields;

    http_headers () = default;

    http_headers (fields_type f)
      : fields (std::move (f)) {}

    void
    set (std::string name, std::string value);

    void
    add (std::string name, std::string value);

    std::optional<std::string>
    get (const std::string& name) const;

    bool
    contains (const std::string& name) const;

    void
    remove (const std::string& name);

    void
    clear () noexcept
    {
      fields.clear ();
    }

    bool
    empty () const noexcept
    {
      return fields.empty ();
    }

    std::size_t
    size () const noexcept
    {
      return fields.size ();
    }

    using iterator = fields_type::iterator;
    using const_iterator = fields_type::const_iterator;

    iterator
    begin () noexcept
    {
      return fields.begin ();
    }

    const_iterator
    begin () const noexcept
    {
      return fields.begin ();
    }

    iterator
    end () noexcept
    {
      return fields.end ();
    }

    const_iterator
    end () const noexcept
    {
      return fields.end ();
    }
  };

  bool
  operator== (const http_headers& x, const http_headers& y) noexcept;

  // HTTP version.
  //
  struct http_version
  {
    std::uint8_t major;
    std::uint8_t minor;

    explicit
    http_version (std::uint8_t maj = 1, std::uint8_t min = 1)
      : major (maj),
        minor (min) {}

    auto
    operator<=> (const http_version&) const noexcept = default;

    std::string
    string () const;
  };

  std::ostream&
  operator << (std::ostream& o, const http_version& v);
}
