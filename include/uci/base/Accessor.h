#pragma once
#include <string>

namespace uci {
namespace base {

class Accessor {
public:
    enum AccessorType {
        ACCESSOR_TYPE_SIMPLE_PRIMITIVE,
        ACCESSOR_TYPE_STRING,
        ACCESSOR_TYPE_UUID,
        ACCESSOR_TYPE_ENUMERATION,
        ACCESSOR_TYPE_COMPLEX,
        ACCESSOR_TYPE_CHOICE,
        ACCESSOR_TYPE_GLOBAL_ELEMENT,
    };

    virtual AccessorType getAccessorType() const = 0;
    virtual void reset() = 0;
    virtual const std::string& typeName() const = 0;

    virtual ~Accessor() = default;

protected:
    Accessor() = default;
    Accessor(const Accessor&) = default;
    Accessor& operator=(const Accessor&) = default;
};

} // namespace base
} // namespace uci
