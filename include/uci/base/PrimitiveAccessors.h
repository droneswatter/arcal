#pragma once

#include "Accessor.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace uci {
namespace base {

template <typename T, accessorType::AccessorType V>
class PrimitiveAccessor : public Accessor {
public:
    using ValueType = T;

    PrimitiveAccessor() = default;
    PrimitiveAccessor(const ValueType& value) : value_(value) {}
    PrimitiveAccessor(ValueType&& value) : value_(std::move(value)) {}
    PrimitiveAccessor(const PrimitiveAccessor&) = default;
    PrimitiveAccessor& operator=(const PrimitiveAccessor&) = default;
    ~PrimitiveAccessor() override = default;

    AccessorType getAccessorType() const noexcept override { return V; }
    void reset() override { value_ = ValueType{}; }
    const std::string& typeName() const override {
        static const std::string kName{"primitive"};
        return kName;
    }

    void copy(const PrimitiveAccessor& rhs) { value_ = rhs.value_; }

    const ValueType& getValue(bool = true) const { return value_; }
    ValueType& getValue() { return value_; }
    void setValue(const ValueType& value) { value_ = value; }
    void setValue(ValueType&& value) { value_ = std::move(value); }

    PrimitiveAccessor& operator=(const ValueType& value) { value_ = value; return *this; }
    PrimitiveAccessor& operator=(ValueType&& value) { value_ = std::move(value); return *this; }
    operator const ValueType&() const { return value_; }

private:
    ValueType value_{};
};

using BooleanAccessor        = PrimitiveAccessor<bool, accessorType::booleanAccessor>;
using ByteAccessor           = PrimitiveAccessor<int8_t, accessorType::byteAccessor>;
using ShortAccessor          = PrimitiveAccessor<int16_t, accessorType::shortAccessor>;
using IntAccessor            = PrimitiveAccessor<int32_t, accessorType::intAccessor>;
using LongAccessor           = PrimitiveAccessor<int64_t, accessorType::longAccessor>;
using FloatAccessor          = PrimitiveAccessor<float, accessorType::floatAccessor>;
using DoubleAccessor         = PrimitiveAccessor<double, accessorType::doubleAccessor>;
using UnsignedByteAccessor   = PrimitiveAccessor<uint8_t, accessorType::byteAccessor>;
using UnsignedShortAccessor  = PrimitiveAccessor<uint16_t, accessorType::shortAccessor>;
using UnsignedIntAccessor    = PrimitiveAccessor<uint32_t, accessorType::intAccessor>;
using UnsignedLongAccessor   = PrimitiveAccessor<uint64_t, accessorType::longAccessor>;
using BinaryAccessor         = PrimitiveAccessor<std::vector<uint8_t>, accessorType::binaryAccessor>;

} // namespace base
} // namespace uci
