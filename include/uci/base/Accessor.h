#pragma once
#include "accessorType.h"
#include <string>

namespace uci { namespace base { class Accessor; } }

namespace arcal { void arcalDestroyAccessor(uci::base::Accessor* acc); }

namespace uci {
namespace base {

class Accessor {
public:
    using AccessorType = uci::base::accessorType::AccessorType;

    // Backward-compat constants (arcal implementation range)
    static constexpr AccessorType ACCESSOR_TYPE_SIMPLE_PRIMITIVE = accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE;
    static constexpr AccessorType ACCESSOR_TYPE_STRING           = accessorType::ACCESSOR_TYPE_STRING;
    static constexpr AccessorType ACCESSOR_TYPE_UUID             = accessorType::ACCESSOR_TYPE_UUID;
    static constexpr AccessorType ACCESSOR_TYPE_ENUMERATION      = accessorType::ACCESSOR_TYPE_ENUMERATION;
    static constexpr AccessorType ACCESSOR_TYPE_COMPLEX          = accessorType::ACCESSOR_TYPE_COMPLEX;
    static constexpr AccessorType ACCESSOR_TYPE_CHOICE           = accessorType::ACCESSOR_TYPE_CHOICE;
    static constexpr AccessorType ACCESSOR_TYPE_GLOBAL_ELEMENT   = accessorType::ACCESSOR_TYPE_GLOBAL_ELEMENT;

    virtual AccessorType getAccessorType() const noexcept = 0;
    virtual void reset() = 0;
    virtual const std::string& typeName() const = 0;

protected:
    friend void ::arcal::arcalDestroyAccessor(uci::base::Accessor* acc);

    virtual ~Accessor() = default;

    Accessor() = default;
    Accessor(const Accessor&) = default;
    Accessor& operator=(const Accessor&) = default;
};

} // namespace base
} // namespace uci
