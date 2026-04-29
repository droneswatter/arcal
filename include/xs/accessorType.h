#pragma once
// OMSC-SPC-008 §4 — C++ namespace xs mirrors the XSD namespace xs.
// xs::String is the C++ representation of xs:string.
// xs::accessorType::ACCESSOR_TYPE_STRING is the BoundedList accessor tag for string-typed elements.
#include "uci/base/accessorType.h"
#include <cstdint>
#include <string>
#include <vector>

namespace xs {

using Boolean = bool;
using Byte = int8_t;
using Short = int16_t;
using Int = int32_t;
using Long = int64_t;
using Float = float;
using Double = double;
using UnsignedByte = uint8_t;
using UnsignedShort = uint16_t;
using UnsignedInt = uint32_t;
using UnsignedLong = uint64_t;
using String = std::string;
using AnyURI = std::string;
// Table 9.1-1 specifies Duration, Time, and DateTime as int64_t (64-bit
// integer nanoseconds/epoch) — NOT as ISO 8601 strings.
// CERT CXX-004937 requires these definitions to match the "Type Definition"
// column of The Simple Primitive Types table.
using Duration = int64_t;
using Time = int64_t;
using DateTime = int64_t;

// DateTimeStamp and Date are NOT in Table 9.1-1 and remain as strings.
using DateTimeStamp = std::string;
using Date = std::string;
using ID = std::string;
using IDREF = std::string;
using NMTOKEN = std::string;
using Binary = std::vector<uint8_t>;

namespace accessorType {
    static constexpr uci::base::accessorType::AccessorType ACCESSOR_TYPE_STRING =
        uci::base::accessorType::ACCESSOR_TYPE_STRING;
} // namespace accessorType

} // namespace xs
