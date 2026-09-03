#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace launcher
{
  namespace pb
  {
    using byte_view   = std::span<const std::uint8_t>;
    using byte_buffer = std::vector<std::uint8_t>;

    enum class wire_type : std::uint8_t
    {
      varint           = 0,
      fixed64          = 1,
      length_delimited = 2,
      start_group      = 3,
      end_group        = 4,
      fixed32          = 5
    };

    std::string
    to_string (wire_type);

    constexpr std::uint32_t max_field_number = 536870911u;

    class decode_error : public std::runtime_error
    {
    public:
      decode_error (std::size_t offset, const std::string& what);

      std::size_t
      offset () const noexcept
      {
        return offset_;
      }

    private:
      std::size_t offset_;
    };

    class reader
    {
    public:
      explicit
      reader (byte_view data) noexcept;

      reader (const reader&) = default;
      reader& operator= (const reader&) = default;

      bool
      next ();

      std::uint32_t
      field_number () const noexcept
      {
        return field_;
      }

      wire_type
      type () const noexcept
      {
        return type_;
      }

      void
      skip ();

      std::uint64_t read_uint64 ();
      std::uint32_t read_uint32 ();
      std::int64_t  read_int64 ();
      std::int32_t  read_int32 ();
      std::int64_t  read_sint64 ();
      std::int32_t  read_sint32 ();
      bool          read_bool ();

      std::uint64_t read_fixed64 ();
      std::uint32_t read_fixed32 ();
      std::int64_t  read_sfixed64 ();
      std::int32_t  read_sfixed32 ();
      float         read_float ();
      double        read_double ();

      std::string_view read_string ();
      byte_view        read_bytes ();

      reader
      read_message ();

      void
      read_packed_uint64 (std::vector<std::uint64_t>& out);

      void
      read_packed_uint32 (std::vector<std::uint32_t>& out);

      std::size_t
      offset () const noexcept
      {
        return position_;
      }

      bool
      exhausted () const noexcept
      {
        return position_ >= data_.size () && consumed_;
      }

    private:
      std::uint64_t
      decode_varint ();

      byte_view
      take (std::size_t n);

      void
      expect (wire_type) const;

      void
      consume ();

      [[noreturn]] void
      fail (const std::string& what) const;

    private:
      byte_view     data_;
      std::size_t   position_;
      std::size_t   field_offset_;
      std::uint32_t field_;
      wire_type     type_;
      bool          consumed_;
      bool          started_;
    };

    class writer
    {
    public:
      writer () = default;

      explicit
      writer (std::size_t reserve);

      void add_uint64 (std::uint32_t field, std::uint64_t);
      void add_uint32 (std::uint32_t field, std::uint32_t);
      void add_int64  (std::uint32_t field, std::int64_t);
      void add_int32  (std::uint32_t field, std::int32_t);
      void add_sint64 (std::uint32_t field, std::int64_t);
      void add_sint32 (std::uint32_t field, std::int32_t);
      void add_bool   (std::uint32_t field, bool);

      void add_fixed64  (std::uint32_t field, std::uint64_t);
      void add_fixed32  (std::uint32_t field, std::uint32_t);
      void add_sfixed64 (std::uint32_t field, std::int64_t);
      void add_sfixed32 (std::uint32_t field, std::int32_t);
      void add_float    (std::uint32_t field, float);
      void add_double   (std::uint32_t field, double);

      void add_string  (std::uint32_t field, std::string_view);
      void add_bytes   (std::uint32_t field, byte_view);
      void add_message (std::uint32_t field, const writer&);
      void add_message (std::uint32_t field, byte_view);

      void add_packed_uint32 (std::uint32_t field,
                              std::span<const std::uint32_t>);

      const byte_buffer&
      data () const noexcept
      {
        return buffer_;
      }

      byte_buffer
      release () noexcept
      {
        return std::move (buffer_);
      }

      byte_view
      view () const noexcept
      {
        return byte_view (buffer_.data (), buffer_.size ());
      }

      std::size_t
      size () const noexcept
      {
        return buffer_.size ();
      }

      bool
      empty () const noexcept
      {
        return buffer_.empty ();
      }

      void
      clear () noexcept
      {
        buffer_.clear ();
      }

    private:
      void
      add_tag (std::uint32_t field, wire_type);

      void
      add_varint (std::uint64_t);

      void
      add_le (std::uint64_t value, std::size_t width);

    private:
      byte_buffer buffer_;
    };

    std::size_t
    varint_size (std::uint64_t) noexcept;

    constexpr std::uint64_t
    zigzag_encode (std::int64_t v) noexcept
    {
      return (static_cast<std::uint64_t> (v) << 1) ^
             static_cast<std::uint64_t> (v >> 63);
    }

    constexpr std::int64_t
    zigzag_decode (std::uint64_t v) noexcept
    {
      return static_cast<std::int64_t> ((v >> 1) ^ (~(v & 1) + 1));
    }
  }
}
