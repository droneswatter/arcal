#pragma once
// OMSC-SPC-008 §10.3 — C++ type aliases for XSD primitive types used in BoundedList element positions.
// These are transparent aliases; BoundedList<double,...> == BoundedList<DoubleAccessor,...>.
#include <cstdint>
#include <string>
#include <vector>

namespace uci {
namespace base {

using BooleanAccessor        = bool;
using ByteAccessor           = int8_t;
using ShortAccessor          = int16_t;
using IntAccessor            = int32_t;
using LongAccessor           = int64_t;
using FloatAccessor          = float;
using DoubleAccessor         = double;
using UnsignedByteAccessor   = uint8_t;
using UnsignedShortAccessor  = uint16_t;
using UnsignedIntAccessor    = uint32_t;
using UnsignedLongAccessor   = uint64_t;
using BinaryAccessor         = std::vector<uint8_t>;

} // namespace base
} // namespace uci
