// Target conformance for generated accessor lifecycle.
//
// This file documents the OMSC-SPC-008 RevK lifecycle surface that ARCAL's
// generated accessors must satisfy after the interface/implementation split.
// It is intentionally not wired into CMake yet because the current generator
// still emits public value-object accessors. Wire this file into
// test/conform/CMakeLists.txt with the generator change that makes it pass.

#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/type/ActionCommandMT.h"
#include "uci/type/AccelerationType.h"

#include <type_traits>

namespace {

using GlobalMsg = uci::type::ActionCommandMT;
using Complex = uci::type::AccelerationType;

static_assert(!std::is_constructible_v<GlobalMsg>,
    "generated global message accessors must not be publicly default-constructible");
static_assert(!std::is_copy_constructible_v<GlobalMsg>,
    "generated global message accessors must not be publicly copy-constructible");
static_assert(!std::is_assignable_v<GlobalMsg&, const GlobalMsg&>,
    "generated global message accessors must not be publicly assignable");
static_assert(!std::is_destructible_v<GlobalMsg>,
    "generated global message accessors must not be publicly destructible");

static_assert(!std::is_constructible_v<Complex>,
    "generated complex accessors must not be publicly default-constructible");
static_assert(!std::is_copy_constructible_v<Complex>,
    "generated complex accessors must not be publicly copy-constructible");
static_assert(!std::is_assignable_v<Complex&, const Complex&>,
    "generated complex accessors must not be publicly assignable");
static_assert(!std::is_destructible_v<Complex>,
    "generated complex accessors must not be publicly destructible");

using GlobalCreateFn = GlobalMsg& (*)(uci::base::AbstractServiceBusConnection*);
using GlobalCopyCreateFn = GlobalMsg& (*)(const GlobalMsg&,
                                          uci::base::AbstractServiceBusConnection*);
using GlobalDestroyFn = void (*)(GlobalMsg&);
using GlobalCopyFn = void (GlobalMsg::*)(const GlobalMsg&);

static_assert(std::is_same_v<GlobalCreateFn,
    decltype(static_cast<GlobalCreateFn>(&GlobalMsg::create))>);
static_assert(std::is_same_v<GlobalCopyCreateFn,
    decltype(static_cast<GlobalCopyCreateFn>(&GlobalMsg::create))>);
static_assert(std::is_same_v<GlobalDestroyFn,
    decltype(static_cast<GlobalDestroyFn>(&GlobalMsg::destroy))>);
static_assert(std::is_same_v<GlobalCopyFn,
    decltype(static_cast<GlobalCopyFn>(&GlobalMsg::copy))>);

using ComplexCreateFn = Complex& (*)(uci::base::AbstractServiceBusConnection*);
using ComplexCopyCreateFn = Complex& (*)(const Complex&,
                                         uci::base::AbstractServiceBusConnection*);
using ComplexDestroyFn = void (*)(Complex&);
using ComplexCopyFn = void (Complex::*)(const Complex&);

static_assert(std::is_same_v<ComplexCreateFn,
    decltype(static_cast<ComplexCreateFn>(&Complex::create))>);
static_assert(std::is_same_v<ComplexCopyCreateFn,
    decltype(static_cast<ComplexCopyCreateFn>(&Complex::create))>);
static_assert(std::is_same_v<ComplexDestroyFn,
    decltype(static_cast<ComplexDestroyFn>(&Complex::destroy))>);
static_assert(std::is_same_v<ComplexCopyFn,
    decltype(static_cast<ComplexCopyFn>(&Complex::copy))>);

} // namespace
