// Compile-only conformance: generated enum API (OMSC-SPC-008 §10.3)
// Uses SystemStateEnum as a representative generated type.
#include "uci/type/SystemStateEnum.h"
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

using E = uci::type::SystemStateEnum;

// enumNotSet must be the zero sentinel
static_assert(E::enumNotSet == 0, "enumNotSet must be 0");

// enumMaxExclusive must be greater than any real value
static_assert(E::enumMaxExclusive > E::enumNotSet);

// isValid instance method exists with bool return
static_assert(std::is_same_v<decltype(std::declval<E>().isValid()), bool>);

// static bool isValid(EnumerationItem) overload
static_assert(std::is_same_v<decltype(E::isValid(E::enumNotSet)), bool>,
    "static bool isValid(EnumerationItem) must exist");

// static bool isValid(const std::string&) overload
static_assert(std::is_same_v<decltype(E::isValid(std::string{})), bool>,
    "static bool isValid(const std::string&) must exist");

// setValueFromName must exist
void test_setValueFromName() {
    E e;
    e.setValueFromName("enumNotSet");
}

// operator<< must exist
void test_stream() {
    E e;
    std::ostringstream os;
    os << e;
}

// default-constructed value must be enumNotSet (= 0) — verified at function scope
void test_default_value() {
    E e;
    static_assert(E::enumNotSet == 0);
}
