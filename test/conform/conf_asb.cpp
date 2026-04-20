// Compile-only conformance: AbstractServiceBusConnection.h (OMSC-SPC-008 §10.4)
#include "uci/base/AbstractServiceBusConnection.h"
#include <string>
#include <type_traits>

using ASB      = uci::base::AbstractServiceBusConnection;
using Listener = uci::base::AbstractServiceBusConnectionStatusListener;

// addStatusListener / removeStatusListener must take Listener& (by reference, not pointer)
using AddFn    = void (ASB::*)(Listener&);
using RemoveFn = void (ASB::*)(Listener&);

static_assert(std::is_same_v<AddFn,    decltype(&ASB::addStatusListener)>,
    "addStatusListener must take Listener& (by reference)");
static_assert(std::is_same_v<RemoveFn, decltype(&ASB::removeStatusListener)>,
    "removeStatusListener must take Listener& (by reference)");

// uci_getAbstractServiceBusConnection must take the spec-shaped two string parameters.
using GetAsbFn = ASB* (*)(const std::string&, const std::string&);
static_assert(std::is_same_v<GetAsbFn, decltype(static_cast<GetAsbFn>(&uci_getAbstractServiceBusConnection))>,
    "uci_getAbstractServiceBusConnection must take two const std::string& parameters");
