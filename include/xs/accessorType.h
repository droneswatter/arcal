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
using DateTime = std::string;
using DateTimeStamp = std::string;
using Date = std::string;
using Time = std::string;
using Duration = std::string;
using ID = std::string;
using IDREF = std::string;
using NMTOKEN = std::string;
using Binary = std::vector<uint8_t>;

namespace accessorType {
    static constexpr uci::base::accessorType::AccessorType ACCESSOR_TYPE_STRING =
        uci::base::accessorType::ACCESSOR_TYPE_STRING;
} // namespace accessorType

} // namespace xs
