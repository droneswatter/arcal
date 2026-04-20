// Compile-only conformance: uci/base/accessorType.h (OMSC-SPC-008 §10.3)
#include "uci/base/accessorType.h"
#include <cstdint>
#include <type_traits>

namespace at = uci::base::accessorType;

static_assert(std::is_same_v<at::AccessorType, uint32_t>,
    "AccessorType must be uint32_t");
static_assert(at::null == 0u, "null must be 0");

// Spec primitive range (0–99)
static_assert(at::booleanAccessor  >= 1u && at::booleanAccessor  < 100u);
static_assert(at::uuidAccessor     >= 1u && at::uuidAccessor     < 100u);
static_assert(at::stringAccessor   >= 1u && at::stringAccessor   < 100u);

// arcal implementation range (100+)
static_assert(at::ACCESSOR_TYPE_SIMPLE_PRIMITIVE >= 100u);
static_assert(at::ACCESSOR_TYPE_ENUMERATION      >= 100u);
static_assert(at::ACCESSOR_TYPE_COMPLEX          >= 100u);
static_assert(at::ACCESSOR_TYPE_CHOICE           >= 100u);
static_assert(at::ACCESSOR_TYPE_GLOBAL_ELEMENT   >= 100u);

// All constants are distinct
static_assert(at::ACCESSOR_TYPE_SIMPLE_PRIMITIVE != at::ACCESSOR_TYPE_STRING);
static_assert(at::ACCESSOR_TYPE_STRING           != at::ACCESSOR_TYPE_UUID);
static_assert(at::ACCESSOR_TYPE_UUID             != at::ACCESSOR_TYPE_ENUMERATION);
static_assert(at::ACCESSOR_TYPE_ENUMERATION      != at::ACCESSOR_TYPE_COMPLEX);
static_assert(at::ACCESSOR_TYPE_COMPLEX          != at::ACCESSOR_TYPE_CHOICE);
static_assert(at::ACCESSOR_TYPE_CHOICE           != at::ACCESSOR_TYPE_GLOBAL_ELEMENT);
