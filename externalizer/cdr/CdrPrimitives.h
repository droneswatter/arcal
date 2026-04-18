#pragma once

// CDR (Common Data Representation, OMG formal/2002-06-51) encode/decode helpers.
// All values stored big-endian; fields aligned to their natural size.
//
// encode_* functions append to a byte vector.
// decode_* functions read from a cursor (offset into a const byte span) and advance it.

#include "uci/base/UCIException.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace arcal {
namespace externalizer {
namespace cdr {

// ---------------------------------------------------------------------------
// Alignment
// ---------------------------------------------------------------------------
inline void align(std::vector<uint8_t>& buf, std::size_t alignment) {
    while (buf.size() % alignment != 0) buf.push_back(0);
}

inline void align(std::size_t& offset, std::size_t alignment) {
    if (alignment > 1 && offset % alignment != 0)
        offset += alignment - (offset % alignment);
}

// ---------------------------------------------------------------------------
// Encode
// ---------------------------------------------------------------------------

inline void encode_bool(std::vector<uint8_t>& buf, bool v) {
    buf.push_back(v ? 1 : 0);
}

inline void encode_uint8(std::vector<uint8_t>& buf, uint8_t v) {
    buf.push_back(v);
}

inline void encode_int8(std::vector<uint8_t>& buf, int8_t v) {
    buf.push_back(static_cast<uint8_t>(v));
}

inline void encode_uint16(std::vector<uint8_t>& buf, uint16_t v) {
    align(buf, 2);
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v));
}

inline void encode_int16(std::vector<uint8_t>& buf, int16_t v) {
    encode_uint16(buf, static_cast<uint16_t>(v));
}

inline void encode_uint32(std::vector<uint8_t>& buf, uint32_t v) {
    align(buf, 4);
    buf.push_back(static_cast<uint8_t>(v >> 24));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >>  8));
    buf.push_back(static_cast<uint8_t>(v));
}

inline void encode_int32(std::vector<uint8_t>& buf, int32_t v) {
    encode_uint32(buf, static_cast<uint32_t>(v));
}

inline void encode_uint64(std::vector<uint8_t>& buf, uint64_t v) {
    align(buf, 8);
    for (int i = 56; i >= 0; i -= 8)
        buf.push_back(static_cast<uint8_t>(v >> i));
}

inline void encode_int64(std::vector<uint8_t>& buf, int64_t v) {
    encode_uint64(buf, static_cast<uint64_t>(v));
}

inline void encode_float(std::vector<uint8_t>& buf, float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    encode_uint32(buf, bits);
}

inline void encode_double(std::vector<uint8_t>& buf, double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, 8);
    encode_uint64(buf, bits);
}

// 4-byte length prefix + UTF-8 bytes + null terminator
inline void encode_string(std::vector<uint8_t>& buf, const std::string& s) {
    encode_uint32(buf, static_cast<uint32_t>(s.size() + 1));
    buf.insert(buf.end(), s.begin(), s.end());
    buf.push_back(0);
}

// Byte sequence: 4-byte length prefix + raw bytes
inline void encode_bytes(std::vector<uint8_t>& buf, const std::vector<uint8_t>& v) {
    encode_uint32(buf, static_cast<uint32_t>(v.size()));
    buf.insert(buf.end(), v.begin(), v.end());
}

// Presence flag for optional fields
inline void encode_optional_flag(std::vector<uint8_t>& buf, bool present) {
    encode_bool(buf, present);
}

// Sequence/list: 4-byte count
inline void encode_sequence_length(std::vector<uint8_t>& buf, uint32_t count) {
    encode_uint32(buf, count);
}

// ---------------------------------------------------------------------------
// Decode
// ---------------------------------------------------------------------------

inline void check_bounds(const std::vector<uint8_t>& buf, std::size_t offset, std::size_t needed) {
    if (offset + needed > buf.size())
        throwUciException("CDR decode: buffer underrun at offset " << offset
                          << " need " << needed << " have " << buf.size());
}

inline bool decode_bool(const std::vector<uint8_t>& buf, std::size_t& off) {
    check_bounds(buf, off, 1);
    return buf[off++] != 0;
}

inline uint8_t decode_uint8(const std::vector<uint8_t>& buf, std::size_t& off) {
    check_bounds(buf, off, 1);
    return buf[off++];
}

inline int8_t decode_int8(const std::vector<uint8_t>& buf, std::size_t& off) {
    return static_cast<int8_t>(decode_uint8(buf, off));
}

inline uint16_t decode_uint16(const std::vector<uint8_t>& buf, std::size_t& off) {
    align(off, 2); check_bounds(buf, off, 2);
    uint16_t v = (static_cast<uint16_t>(buf[off]) << 8) | buf[off+1];
    off += 2; return v;
}

inline int16_t decode_int16(const std::vector<uint8_t>& buf, std::size_t& off) {
    return static_cast<int16_t>(decode_uint16(buf, off));
}

inline uint32_t decode_uint32(const std::vector<uint8_t>& buf, std::size_t& off) {
    align(off, 4); check_bounds(buf, off, 4);
    uint32_t v = (static_cast<uint32_t>(buf[off  ]) << 24) |
                 (static_cast<uint32_t>(buf[off+1]) << 16) |
                 (static_cast<uint32_t>(buf[off+2]) <<  8) |
                  static_cast<uint32_t>(buf[off+3]);
    off += 4; return v;
}

inline int32_t decode_int32(const std::vector<uint8_t>& buf, std::size_t& off) {
    return static_cast<int32_t>(decode_uint32(buf, off));
}

inline uint64_t decode_uint64(const std::vector<uint8_t>& buf, std::size_t& off) {
    align(off, 8); check_bounds(buf, off, 8);
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | buf[off+i];
    off += 8; return v;
}

inline int64_t decode_int64(const std::vector<uint8_t>& buf, std::size_t& off) {
    return static_cast<int64_t>(decode_uint64(buf, off));
}

inline float decode_float(const std::vector<uint8_t>& buf, std::size_t& off) {
    uint32_t bits = decode_uint32(buf, off);
    float v; std::memcpy(&v, &bits, 4); return v;
}

inline double decode_double(const std::vector<uint8_t>& buf, std::size_t& off) {
    uint64_t bits = decode_uint64(buf, off);
    double v; std::memcpy(&v, &bits, 8); return v;
}

inline std::string decode_string(const std::vector<uint8_t>& buf, std::size_t& off) {
    uint32_t len = decode_uint32(buf, off);
    if (len == 0) return {};
    check_bounds(buf, off, len);
    std::string s(reinterpret_cast<const char*>(buf.data() + off), len - 1); // exclude null
    off += len;
    return s;
}

inline std::vector<uint8_t> decode_bytes(const std::vector<uint8_t>& buf, std::size_t& off) {
    uint32_t len = decode_uint32(buf, off);
    check_bounds(buf, off, len);
    std::vector<uint8_t> v(buf.data() + off, buf.data() + off + len);
    off += len;
    return v;
}

inline bool decode_optional_flag(const std::vector<uint8_t>& buf, std::size_t& off) {
    return decode_bool(buf, off);
}

inline uint32_t decode_sequence_length(const std::vector<uint8_t>& buf, std::size_t& off) {
    return decode_uint32(buf, off);
}

} // namespace cdr
} // namespace externalizer
} // namespace arcal
