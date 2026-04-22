#pragma once

#include "uci/base/UCIException.h"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <exception>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace arcal {
namespace externalizer {
namespace json {

inline const nlohmann::json& require_object(const nlohmann::json& value, const char* context) {
    if (!value.is_object()) {
        throwUciException("JsonExternalizer::read: expected object for " << context);
    }
    return value;
}

inline const nlohmann::json& require_member(const nlohmann::json& object,
                                            const char* name,
                                            const char* context) {
    require_object(object, context);
    auto it = object.find(name);
    if (it == object.end()) {
        throwUciException("JsonExternalizer::read: missing required field "
                          << context << "." << name);
    }
    return *it;
}

inline const nlohmann::json* optional_member(const nlohmann::json& object,
                                             const char* name,
                                             const char* context) {
    require_object(object, context);
    auto it = object.find(name);
    return it == object.end() ? nullptr : &*it;
}

inline void require_array(const nlohmann::json& value, const char* context) {
    if (!value.is_array()) {
        throwUciException("JsonExternalizer::read: expected array for " << context);
    }
}

inline std::vector<uint8_t> parse_hex_bytes(const nlohmann::json& value, const char* context) {
    if (!value.is_string()) {
        throwUciException("JsonExternalizer::read: expected hex string for " << context);
    }
    const auto text = value.get<std::string>();
    if ((text.size() % 2) != 0) {
        throwUciException("JsonExternalizer::read: odd-length hex string for " << context);
    }

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        return -1;
    };

    std::vector<uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int hi = nibble(text[i]);
        const int lo = nibble(text[i + 1]);
        if (hi < 0 || lo < 0) {
            throwUciException("JsonExternalizer::read: invalid hex string for " << context);
        }
        bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return bytes;
}

template <typename T>
T parse_value(const nlohmann::json& value, const char* context) {
    if constexpr (std::is_same_v<T, bool>) {
        if (!value.is_boolean()) {
            throwUciException("JsonExternalizer::read: expected boolean for " << context);
        }
        return value.get<bool>();
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!value.is_string()) {
            throwUciException("JsonExternalizer::read: expected string for " << context);
        }
        return value.get<std::string>();
    } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
        return parse_hex_bytes(value, context);
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
        if (!value.is_number_integer()) {
            throwUciException("JsonExternalizer::read: expected signed integer for " << context);
        }
        const auto parsed = value.get<int64_t>();
        if (parsed < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
            parsed > static_cast<int64_t>(std::numeric_limits<T>::max())) {
            throwUciException("JsonExternalizer::read: integer out of range for " << context);
        }
        return static_cast<T>(parsed);
    } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
        if (!value.is_number_unsigned() && !value.is_number_integer()) {
            throwUciException("JsonExternalizer::read: expected unsigned integer for " << context);
        }
        uint64_t parsed = 0;
        if (value.is_number_unsigned()) {
            parsed = value.get<uint64_t>();
        } else {
            const auto signedParsed = value.get<int64_t>();
            if (signedParsed < 0) {
                throwUciException("JsonExternalizer::read: negative unsigned integer for " << context);
            }
            parsed = static_cast<uint64_t>(signedParsed);
        }
        if (parsed > static_cast<uint64_t>(std::numeric_limits<T>::max())) {
            throwUciException("JsonExternalizer::read: unsigned integer out of range for " << context);
        }
        return static_cast<T>(parsed);
    } else if constexpr (std::is_floating_point_v<T>) {
        if (!value.is_number()) {
            throwUciException("JsonExternalizer::read: expected number for " << context);
        }
        return value.get<T>();
    } else {
        static_assert(!sizeof(T), "Unsupported JSON primitive type");
    }
}

} // namespace json
} // namespace externalizer
} // namespace arcal
