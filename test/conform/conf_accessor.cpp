// Compile-only conformance: uci/base/Accessor.h (OMSC-SPC-008 §10.3)
#include "uci/base/Accessor.h"
#include <string>
#include <type_traits>

// AccessorType in class must alias the namespace type
static_assert(std::is_same_v<uci::base::Accessor::AccessorType, uint32_t>);

// Backward-compat class constants must equal namespace constants
static_assert(uci::base::Accessor::ACCESSOR_TYPE_COMPLEX
           == uci::base::accessorType::ACCESSOR_TYPE_COMPLEX);
static_assert(uci::base::Accessor::ACCESSOR_TYPE_ENUMERATION
           == uci::base::accessorType::ACCESSOR_TYPE_ENUMERATION);

// Accessor is abstract (pure virtual methods — not directly instantiable)
static_assert(!std::is_constructible_v<uci::base::Accessor>);

// Concrete subclass to verify getAccessorType() noexcept contract
struct ConcreteAccessor : uci::base::Accessor {
    AccessorType getAccessorType() const noexcept override { return ACCESSOR_TYPE_COMPLEX; }
    void reset() override {}
    const std::string& typeName() const override {
        static const std::string n{"concrete"};
        return n;
    }
};

static_assert(noexcept(std::declval<ConcreteAccessor>().getAccessorType()),
    "getAccessorType() must be noexcept");
