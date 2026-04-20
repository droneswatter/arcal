#pragma once
// OMSC-SPC-008 §10.3 — uci::base::accessorType
#include <cstdint>

namespace uci {
namespace base {
namespace accessorType {

using AccessorType = uint32_t;

// Spec-defined primitive accessor constants
static constexpr AccessorType null             = 0;
static constexpr AccessorType booleanAccessor  = 1;
static constexpr AccessorType byteAccessor     = 2;
static constexpr AccessorType shortAccessor    = 3;
static constexpr AccessorType intAccessor      = 4;
static constexpr AccessorType longAccessor     = 5;
static constexpr AccessorType floatAccessor    = 6;
static constexpr AccessorType doubleAccessor   = 7;
static constexpr AccessorType stringAccessor   = 8;
static constexpr AccessorType dateTimeAccessor = 9;
static constexpr AccessorType durationAccessor = 10;
static constexpr AccessorType uuidAccessor     = 11;
static constexpr AccessorType binaryAccessor   = 12;

// arcal implementation constants (above spec primitive range)
static constexpr AccessorType ACCESSOR_TYPE_SIMPLE_PRIMITIVE = 100;
static constexpr AccessorType ACCESSOR_TYPE_STRING           = 101;
static constexpr AccessorType ACCESSOR_TYPE_UUID             = 102;
static constexpr AccessorType ACCESSOR_TYPE_ENUMERATION      = 103;
static constexpr AccessorType ACCESSOR_TYPE_COMPLEX          = 104;
static constexpr AccessorType ACCESSOR_TYPE_CHOICE           = 105;
static constexpr AccessorType ACCESSOR_TYPE_GLOBAL_ELEMENT   = 106;

} // namespace accessorType
} // namespace base
} // namespace uci
