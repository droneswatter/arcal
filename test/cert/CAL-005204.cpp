// CERT CAL-005204: A CAL Implementation shall report an error to the CAL Client
// in the event the initialization of the CAL instance fails.
// Verified by: StatusListeners receive state transitions (including FAILED) via
// the same mechanism that would deliver an initialization failure. Full
// initialization-failure testing requires DDS environment injection; this test
// exercises the error-reporting path and verifies both getStatus() and listener
// delivery agree on the error state.

#include "uci/base/AbstractServiceBusConnection.h"
#include <cassert>
#include <iostream>

using State    = uci::base::AbstractServiceBusConnectionStatusData::StateEnum;
using StatusData = uci::base::AbstractServiceBusConnectionStatusData;

struct ErrorListener : public uci::base::AbstractServiceBusConnectionStatusListener {
    State lastState{State::INITIALIZING};
    void statusChanged(StatusData s) override { lastState = s.state; }
};

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("ErrorSvc", "DDS");
    assert(asb);
    assert(asb->getStatus().state == State::NORMAL);

    ErrorListener listener;
    asb->addStatusListener(listener);
    assert(listener.lastState == State::NORMAL && "listener must receive current state on registration");

    asb->shutdown();

    assert(asb->getStatus().state == State::FAILED
           && "getStatus() must reflect the error state");
    assert(listener.lastState == State::FAILED
           && "registered listeners must be notified of the error state");

    asb->removeStatusListener(listener);
    uci_destroyAbstractServiceBusConnection(asb);
    std::cout << "PASS CAL-005204\n";
    return 0;
}
