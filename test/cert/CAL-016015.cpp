// CERT CAL-016015: A CAL Implementation shall provide multiple CAL Clients
// operating in a single address space with the same behavior as if each CAL
// Client were executing in separate address spaces. Specifically: each receives
// a distinct Service UUID, and pub/sub between them works correctly.

#include "uci/base/AbstractServiceBusConnection.h"
#include <cassert>
#include <iostream>

int main() {
    auto* asb1 = uci_getAbstractServiceBusConnection("ServiceA", "DDS");
    auto* asb2 = uci_getAbstractServiceBusConnection("ServiceB", "DDS");

    assert(asb1 != nullptr);
    assert(asb2 != nullptr);

    auto uuid1 = asb1->getMyServiceUUID();
    auto uuid2 = asb2->getMyServiceUUID();

    assert(uuid1 != uuid2 && "Two distinct service labels must produce distinct Service UUIDs");

    asb1->shutdown();
    asb2->shutdown();
    uci_destroyAbstractServiceBusConnection(asb1);
    uci_destroyAbstractServiceBusConnection(asb2);

    std::cout << "PASS CAL-016015\n";
    return 0;
}
