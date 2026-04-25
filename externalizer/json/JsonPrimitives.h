#pragma once
// JSON value/key encoding helpers — no heap allocation beyond the output string.

#include "uci/base/UUID.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace arcal {
namespace externalizer {
namespace json {

// Append comma separator if needed, then "key":
inline void emit_key(const char* name, std::string& out, bool& first) {
    if (!first) out += ',';
    first = false;
    out += '"';
    out += name;
    out += "\":";
}

inline void emit_string(const std::string& s, std::string& out) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    out += '"';
}

inline void emit_value(bool v,     std::string& out) { out += v ? "true" : "false"; }
inline void emit_value(int8_t  v,  std::string& out) { out += std::to_string(v); }
inline void emit_value(uint8_t v,  std::string& out) { out += std::to_string(v); }
inline void emit_value(int16_t v,  std::string& out) { out += std::to_string(v); }
inline void emit_value(uint16_t v, std::string& out) { out += std::to_string(v); }
inline void emit_value(int32_t v,  std::string& out) { out += std::to_string(v); }
inline void emit_value(uint32_t v, std::string& out) { out += std::to_string(v); }
inline void emit_value(int64_t v,  std::string& out) { out += std::to_string(v); }
inline void emit_value(uint64_t v, std::string& out) { out += std::to_string(v); }

inline void emit_value(float v, std::string& out) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", static_cast<double>(v));
    out += buf;
}

inline void emit_value(double v, std::string& out) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.17g", v);
    out += buf;
}

inline void emit_value(const std::string& v, std::string& out) { emit_string(v, out); }
inline void emit_value(const uci::base::UUID& v, std::string& out) { emit_string(v.toString(), out); }

// bytes → lowercase hex string
inline void emit_value(const std::vector<uint8_t>& v, std::string& out) {
    static constexpr char hex[] = "0123456789abcdef";
    out += '"';
    for (uint8_t b : v) {
        out += hex[b >> 4];
        out += hex[b & 0x0F];
    }
    out += '"';
}

} // namespace json
} // namespace externalizer
} // namespace arcal
