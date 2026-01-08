#include <launcher/http/http-types.hxx>

#include <stdexcept>
#include <algorithm>
#include <sstream>

using namespace std;

namespace launcher
{
  // http_method
  //
  string
  to_string (http_method m)
  {
    switch (m)
    {
      case http_method::get:     return "GET";
      case http_method::head:    return "HEAD";
      case http_method::post:    return "POST";
      case http_method::put:     return "PUT";
      case http_method::delete_: return "DELETE";
      case http_method::connect: return "CONNECT";
      case http_method::options: return "OPTIONS";
      case http_method::trace:   return "TRACE";
      case http_method::patch:   return "PATCH";
    }
    return "GET";
  }

  http_method
  to_http_method (const string& s)
  {
    string upper;
    upper.reserve (s.size ());
    transform (s.begin (), s.end (), back_inserter (upper),
              [] (unsigned char c) { return toupper (c); });

    if (upper == "GET")     return http_method::get;
    if (upper == "HEAD")    return http_method::head;
    if (upper == "POST")    return http_method::post;
    if (upper == "PUT")     return http_method::put;
    if (upper == "DELETE")  return http_method::delete_;
    if (upper == "CONNECT") return http_method::connect;
    if (upper == "OPTIONS") return http_method::options;
    if (upper == "TRACE")   return http_method::trace;
    if (upper == "PATCH")   return http_method::patch;

    throw invalid_argument ("invalid HTTP method: " + s);
  }

  // http_status
  //
  string
  to_string (http_status s)
  {
    switch (s)
    {
      // 1xx
      //
      case http_status::continue_:                       return "Continue";
      case http_status::switching_protocols:             return "Switching Protocols";
      case http_status::processing:                      return "Processing";
      case http_status::early_hints:                     return "Early Hints";

      // 2xx
      //
      case http_status::ok:                              return "OK";
      case http_status::created:                         return "Created";
      case http_status::accepted:                        return "Accepted";
      case http_status::non_authoritative_information:   return "Non-Authoritative Information";
      case http_status::no_content:                      return "No Content";
      case http_status::reset_content:                   return "Reset Content";
      case http_status::partial_content:                 return "Partial Content";
      case http_status::multi_status:                    return "Multi-Status";
      case http_status::already_reported:                return "Already Reported";
      case http_status::im_used:                         return "IM Used";

      // 3xx
      //
      case http_status::multiple_choices:                return "Multiple Choices";
      case http_status::moved_permanently:               return "Moved Permanently";
      case http_status::found:                           return "Found";
      case http_status::see_other:                       return "See Other";
      case http_status::not_modified:                    return "Not Modified";
      case http_status::use_proxy:                       return "Use Proxy";
      case http_status::temporary_redirect:              return "Temporary Redirect";
      case http_status::permanent_redirect:              return "Permanent Redirect";

      // 4xx
      //
      case http_status::bad_request:                     return "Bad Request";
      case http_status::unauthorized:                    return "Unauthorized";
      case http_status::payment_required:                return "Payment Required";
      case http_status::forbidden:                       return "Forbidden";
      case http_status::not_found:                       return "Not Found";
      case http_status::method_not_allowed:              return "Method Not Allowed";
      case http_status::not_acceptable:                  return "Not Acceptable";
      case http_status::proxy_authentication_required:   return "Proxy Authentication Required";
      case http_status::request_timeout:                 return "Request Timeout";
      case http_status::conflict:                        return "Conflict";
      case http_status::gone:                            return "Gone";
      case http_status::length_required:                 return "Length Required";
      case http_status::precondition_failed:             return "Precondition Failed";
      case http_status::payload_too_large:               return "Payload Too Large";
      case http_status::uri_too_long:                    return "URI Too Long";
      case http_status::unsupported_media_type:          return "Unsupported Media Type";
      case http_status::range_not_satisfiable:           return "Range Not Satisfiable";
      case http_status::expectation_failed:              return "Expectation Failed";
      case http_status::im_a_teapot:                     return "I'm a teapot";
      case http_status::misdirected_request:             return "Misdirected Request";
      case http_status::unprocessable_entity:            return "Unprocessable Entity";
      case http_status::locked:                          return "Locked";
      case http_status::failed_dependency:               return "Failed Dependency";
      case http_status::too_early:                       return "Too Early";
      case http_status::upgrade_required:                return "Upgrade Required";
      case http_status::precondition_required:           return "Precondition Required";
      case http_status::too_many_requests:               return "Too Many Requests";
      case http_status::request_header_fields_too_large: return "Request Header Fields Too Large";
      case http_status::unavailable_for_legal_reasons:   return "Unavailable For Legal Reasons";

      // 5xx
      //
      case http_status::internal_server_error:           return "Internal Server Error";
      case http_status::not_implemented:                 return "Not Implemented";
      case http_status::bad_gateway:                     return "Bad Gateway";
      case http_status::service_unavailable:             return "Service Unavailable";
      case http_status::gateway_timeout:                 return "Gateway Timeout";
      case http_status::http_version_not_supported:      return "HTTP Version Not Supported";
      case http_status::variant_also_negotiates:         return "Variant Also Negotiates";
      case http_status::insufficient_storage:            return "Insufficient Storage";
      case http_status::loop_detected:                   return "Loop Detected";
      case http_status::not_extended:                    return "Not Extended";
      case http_status::network_authentication_required: return "Network Authentication Required";
    }

    return "Unknown";
  }

  // http_version
  //
  string http_version::
  string () const
  {
    ostringstream os;

    os << "HTTP/" << static_cast<unsigned> (major)
       << '.'     << static_cast<unsigned> (minor);

    return os.str ();
  }
}
