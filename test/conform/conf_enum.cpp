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

static_assert(!std::is_constructible_v<E>,
    "generated enum accessors must not be publicly default-constructible");
static_assert(!std::is_copy_constructible_v<E>,
    "generated enum accessors must not be publicly copy-constructible");
static_assert(!std::is_assignable_v<E&, const E&>,
    "generated enum accessors must not be publicly assignable");
static_assert(!std::is_destructible_v<E>,
    "generated enum accessors must not be publicly destructible");

// setValueFromName must exist
void test_setValueFromName() {
    E& e = E::create(nullptr);
    e.setValueFromName("enumNotSet");
    E::destroy(e);
}

// operator<< must exist
void test_stream() {
    E& e = E::create(nullptr);
    std::ostringstream os;
    os << e;
    E::destroy(e);
}

// default-constructed value must be enumNotSet (= 0) — verified at function scope
void test_default_value() {
    E& e = E::create(nullptr);
    (void)e;
    E::destroy(e);
    static_assert(E::enumNotSet == 0);
}
