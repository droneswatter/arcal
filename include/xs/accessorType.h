#pragma once
// OMSC-SPC-008 §4 — C++ namespace xs mirrors the XSD namespace xs.
// xs::String is the C++ representation of xs:string.
// xs::accessorType::ACCESSOR_TYPE_STRING is the BoundedList accessor tag for string-typed elements.
#include "uci/base/accessorType.h"
#include <string>

namespace xs {

using String = std::string;

namespace accessorType {
    static constexpr uci::base::accessorType::AccessorType ACCESSOR_TYPE_STRING =
        uci::base::accessorType::ACCESSOR_TYPE_STRING;
} // namespace accessorType

} // namespace xs
