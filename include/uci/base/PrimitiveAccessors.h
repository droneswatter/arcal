#pragma once

#include "Accessor.h"
#include "xs/accessorType.h"

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

private:
    ValueType value_{};
};

#define ARCAL_DEFINE_PRIMITIVE_ACCESSOR(ClassName, ValueAlias, AccessorTag, MethodName) \
class ClassName : public PrimitiveAccessor<ValueAlias, AccessorTag> { \
public: \
    using Base = PrimitiveAccessor<ValueAlias, AccessorTag>; \
    using Base::Base; \
    ClassName() = default; \
    ClassName(const ClassName&) = default; \
    ClassName& operator=(const ClassName&) = default; \
    ~ClassName() override = default; \
    ValueAlias get##MethodName##Value() const { return Base::getValue(); } \
    ClassName& set##MethodName##Value(ValueAlias value) { Base::setValue(std::move(value)); return *this; } \
    ClassName& operator=(ValueAlias value) { Base::setValue(std::move(value)); return *this; } \
    operator ValueAlias() const { return Base::getValue(); } \
}

ARCAL_DEFINE_PRIMITIVE_ACCESSOR(BooleanAccessor, xs::Boolean, accessorType::booleanAccessor, Boolean);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(ByteAccessor, xs::Byte, accessorType::byteAccessor, Byte);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(ShortAccessor, xs::Short, accessorType::shortAccessor, Short);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(IntAccessor, xs::Int, accessorType::intAccessor, Int);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(LongAccessor, xs::Long, accessorType::longAccessor, Long);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(FloatAccessor, xs::Float, accessorType::floatAccessor, Float);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(DoubleAccessor, xs::Double, accessorType::doubleAccessor, Double);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(UnsignedByteAccessor, xs::UnsignedByte, accessorType::byteAccessor, UnsignedByte);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(UnsignedShortAccessor, xs::UnsignedShort, accessorType::shortAccessor, UnsignedShort);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(UnsignedIntAccessor, xs::UnsignedInt, accessorType::intAccessor, UnsignedInt);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(UnsignedLongAccessor, xs::UnsignedLong, accessorType::longAccessor, UnsignedLong);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(BinaryAccessor, xs::Binary, accessorType::binaryAccessor, Binary);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(DurationAccessor, xs::Duration, accessorType::durationAccessor, Duration);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(TimeAccessor, xs::Time, accessorType::dateTimeAccessor, Time);
ARCAL_DEFINE_PRIMITIVE_ACCESSOR(DateTimeAccessor, xs::DateTime, accessorType::dateTimeAccessor, DateTime);

#undef ARCAL_DEFINE_PRIMITIVE_ACCESSOR

} // namespace base
} // namespace uci
