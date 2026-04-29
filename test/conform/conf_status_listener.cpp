// Compile-only conformance: AbstractServiceBusConnectionStatusListener.h (OMSC-SPC-008 §10.4)
#include "uci/base/AbstractServiceBusConnectionStatusListener.h"
#include <string>
#include <type_traits>

using ASB = uci::base::AbstractServiceBusConnection;
using StatusData = ASB::AbstractServiceBusConnectionStatusData;
using Listener   = uci::base::AbstractServiceBusConnectionStatusListener;

// stateDetail field must exist with type std::string (not 'detail')
static_assert(std::is_same_v<decltype(std::declval<StatusData>().stateDetail), std::string>);

// statusChanged must accept StatusData by value (not const-ref)
// A concrete subclass with the correct signature must compile.
struct ConcreteListener : Listener {
    StatusData received;
    void statusChanged(StatusData s) override { received = s; }
};

// Verify the override signature compiles (wrong sig = won't override = pure-virtual error at link)
static_assert(std::is_base_of_v<Listener, ConcreteListener>);
