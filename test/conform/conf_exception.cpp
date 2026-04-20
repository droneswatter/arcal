// Compile-only conformance: uci/base/UCIException.h (OMSC-SPC-008 §10.2)
#include "uci/base/UCIException.h"
#include <cstdint>
#include <type_traits>

using Ex = uci::base::UCIException;

// ErrorCode must be uint32_t
static_assert(std::is_same_v<Ex::ErrorCode, uint32_t>);

// Required constructors accept an optional ErrorCode.
static_assert(std::is_constructible_v<Ex, const std::string&, Ex::ErrorCode>);
static_assert(std::is_constructible_v<Ex, const char*, Ex::ErrorCode>);
static_assert(std::is_constructible_v<Ex, const std::ostringstream&, Ex::ErrorCode>);

// getErrorCode must be noexcept
static_assert(noexcept(std::declval<Ex>().getErrorCode()),
    "getErrorCode() must be noexcept");

// errorCode_ field type must be ErrorCode
static_assert(std::is_same_v<decltype(std::declval<Ex>().getErrorCode()), Ex::ErrorCode>);

// Must derive from std::runtime_error
static_assert(std::is_base_of_v<std::runtime_error, Ex>);

// throwUciException macro must compile
void test_macro() {
    try {
        throwUciException("test error");
    } catch (const uci::base::UCIException&) {}
}

void test_error_code() {
    Ex e("test error", 42);
    if (e.getErrorCode() != 42) {
        throw Ex("error code was not preserved");
    }
}
