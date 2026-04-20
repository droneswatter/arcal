// CERT CAL-005201: A CAL Implementation shall provide a mechanism for a CAL
// Client to obtain an initialized instance of the CAL Implementation.

#include "uci/base/AbstractServiceBusConnection.h"
#include <cassert>
#include <iostream>

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("InitTest", "DDS");
    assert(asb != nullptr && "uci_getAbstractServiceBusConnection must return non-null");

    // Instance must be in NORMAL state after initialization
    auto status = asb->getStatus();
    assert(status.state == uci::base::AbstractServiceBusConnectionStatusData::NORMAL
           && "ASB must be NORMAL after successful initialization");

    // Service UUID must be valid (non-nil)
    assert(asb->getMyServiceUUID().isValid() && "Service UUID must be valid after init");

    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    std::cout << "PASS CAL-005201\n";
    return 0;
}
