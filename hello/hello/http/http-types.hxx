#pragma once

#include <string>
#include <cstdint>
#include <cassert>
#include <utility>
#include <ostream>
#include <map>
#include <vector>

namespace hello
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

  inline std::ostream&
  operator<< (std::ostream& o, http_method m)
  {
    return o << to_string (m);
  }

  // HTTP status code.
  //
  enum class http_status : std::uint16_t
  {
    // Informational (1xx)
    continue_                     = 100,
    switching_protocols           = 101,
    processing                    = 102,
    early_hints                   = 103,

    // Success (2xx)
    ok                            = 200,
    created                       = 201,
    accepted                      = 202,
    non_authoritative_information = 203,
    no_content                    = 204,
    reset_content                 = 205,
    partial_content               = 206,
    multi_status                  = 207,
    already_reported              = 208,
    im_used                       = 226,

    // Redirection (3xx)
    multiple_choices              = 300,
    moved_permanently             = 301,
    found                         = 302,
    see_other                     = 303,
    not_modified                  = 304,
    use_proxy                     = 305,
    temporary_redirect            = 307,
    permanent_redirect            = 308,

    // Client error (4xx)
    bad_request                   = 400,
    unauthorized                  = 401,
    payment_required              = 402,
    forbidden                     = 403,
    not_found                     = 404,
    method_not_allowed            = 405,
    not_acceptable                = 406,
    proxy_authentication_required = 407,
    request_timeout               = 408,
    conflict                      = 409,
    gone                          = 410,
    length_required               = 411,
    precondition_failed           = 412,
    payload_too_large             = 413,
    uri_too_long                  = 414,
    unsupported_media_type        = 415,
    range_not_satisfiable         = 416,
    expectation_failed            = 417,
    im_a_teapot                   = 418,
    misdirected_request           = 421,
    unprocessable_entity          = 422,
    locked                        = 423,
    failed_dependency             = 424,
    too_early                     = 425,
    upgrade_required              = 426,
    precondition_required         = 428,
    too_many_requests             = 429,
    request_header_fields_too_large = 431,
    unavailable_for_legal_reasons = 451,

    // Server error (5xx)
    internal_server_error         = 500,
    not_implemented               = 501,
    bad_gateway                   = 502,
    service_unavailable           = 503,
    gateway_timeout               = 504,
    http_version_not_supported    = 505,
    variant_also_negotiates       = 506,
    insufficient_storage          = 507,
    loop_detected                 = 508,
    not_extended                  = 510,
    network_authentication_required = 511
  };

  std::string
  to_string (http_status);

  inline std::ostream&
  operator<< (std::ostream& o, http_status s)
  {
    return o << static_cast<std::uint16_t> (s);
  }

  // HTTP header field.
  //
  template <typename S>
  struct basic_http_field
  {
    using string_type = S;

    string_type name;
    string_type value;

    basic_http_field () = default;

    basic_http_field (string_type n, string_type v)
        : name (std::move (n)), value (std::move (v)) {}

    bool
    empty () const noexcept
    {
      return name.empty () && value.empty ();
    }
  };

  template <typename S>
  inline bool
  operator== (const basic_http_field<S>& x, const basic_http_field<S>& y) noexcept
  {
    return x.name == y.name && x.value == y.value;
  }

  template <typename S>
  inline bool
  operator!= (const basic_http_field<S>& x, const basic_http_field<S>& y) noexcept
  {
    return !(x == y);
  }

  // HTTP headers collection.
  //
  template <typename S>
  struct basic_http_headers
  {
    using string_type = S;
    using field_type  = basic_http_field<string_type>;
    using fields_type = std::vector<field_type>;

    fields_type fields;

    basic_http_headers () = default;
    basic_http_headers (fields_type f) : fields (std::move (f)) {}

    // Set a header field, replacing any existing field with the same name.
    //
    void
    set (string_type name, string_type value);

    // Add a header field (allows duplicates).
    //
    void
    add (string_type name, string_type value);

    // Get a header field value. Return nullopt if not found.
    //
    std::optional<string_type>
    get (const string_type& name) const;

    // Check if a header field exists.
    //
    bool
    contains (const string_type& name) const;

    // Remove all fields with the given name.
    //
    void
    remove (const string_type& name);

    // Clear all fields.
    //
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

    // Iterators.
    //
    using iterator       = typename fields_type::iterator;
    using const_iterator = typename fields_type::const_iterator;

    iterator       begin ()       noexcept { return fields.begin (); }
    const_iterator begin () const noexcept { return fields.begin (); }
    iterator       end ()         noexcept { return fields.end (); }
    const_iterator end ()   const noexcept { return fields.end (); }
  };

  template <typename S>
  inline bool
  operator== (const basic_http_headers<S>& x, const basic_http_headers<S>& y) noexcept
  {
    return x.fields == y.fields;
  }

  template <typename S>
  inline bool
  operator!= (const basic_http_headers<S>& x, const basic_http_headers<S>& y) noexcept
  {
    return !(x == y);
  }

  // Common typedefs.
  //
  using http_field   = basic_http_field<std::string>;
  using http_headers = basic_http_headers<std::string>;

  // HTTP version.
  //
  struct http_version
  {
    std::uint8_t major;
    std::uint8_t minor;

    http_version (std::uint8_t maj = 1, std::uint8_t min = 1)
        : major (maj), minor (min) {}

    bool
    operator== (const http_version& v) const noexcept
    {
      return major == v.major && minor == v.minor;
    }

    bool
    operator!= (const http_version& v) const noexcept
    {
      return !(*this == v);
    }

    bool
    operator< (const http_version& v) const noexcept
    {
      return major < v.major || (major == v.major && minor < v.minor);
    }

    std::string
    string () const;
  };

  inline std::ostream&
  operator<< (std::ostream& o, const http_version& v)
  {
    return o << v.string ();
  }
}

#include <hello/http/http-types.ixx>
