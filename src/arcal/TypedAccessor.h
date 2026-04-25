#pragma once

#include <cstdint>

namespace arcal {
namespace type {

class TypedAccessor {
public:
    virtual uint32_t typeTag() const noexcept = 0;

protected:
    virtual ~TypedAccessor() = default;
};

} // namespace type
} // namespace arcal
