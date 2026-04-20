// CERT CAL-016366: Upon successful completion of a request to register as an
// ASB Connection Status listener, a CAL Implementation shall call the registered
// class' API notification method with the current state of the ASB connection.

#include "uci/base/AbstractServiceBusConnection.h"
#include <cassert>
#include <iostream>

struct TestListener : public uci::base::AbstractServiceBusConnectionStatusListener {
    int callCount{0};
    uci::base::AbstractServiceBusConnectionStatusData lastStatus;

    void statusChanged(uci::base::AbstractServiceBusConnectionStatusData s) override {
        ++callCount;
        lastStatus = s;
    }
};

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("StatusTest", "DDS");
    assert(asb != nullptr);

    TestListener listener;
    assert(listener.callCount == 0);

    asb->addStatusListener(listener);

    // Must be called synchronously (or before addStatusListener returns) with current state
    assert(listener.callCount >= 1 && "Listener must be called immediately upon registration");
    assert(listener.lastStatus.state == uci::base::AbstractServiceBusConnectionStatusData::NORMAL);

    asb->removeStatusListener(listener);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    std::cout << "PASS CAL-016366\n";
    return 0;
}
