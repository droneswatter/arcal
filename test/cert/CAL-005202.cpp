// CERT CAL-005202: A CAL Implementation shall associate a single CAL instance
// with each unique combination of Service Identifier and ASB Identifier.
// Verified by: the same service label must produce the same Service UUID
// across independent factory calls, and a different label must produce a
// different UUID.

#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/UUID.h"
#include <cassert>
#include <iostream>

int main() {
    auto* a = uci_getAbstractServiceBusConnection("SvcSame", "DDS");
    auto* b = uci_getAbstractServiceBusConnection("SvcSame", "DDS");
    assert(a && b);

    assert(a->getMyServiceUUID() == b->getMyServiceUUID()
           && "identical service labels must map to the same Service UUID");

    auto* c = uci_getAbstractServiceBusConnection("SvcOther", "DDS");
    assert(c);
    assert(a->getMyServiceUUID() != c->getMyServiceUUID()
           && "different service labels must map to different Service UUIDs");

    for (auto* p : {a, b, c}) { p->shutdown(); uci_destroyAbstractServiceBusConnection(p); }
    std::cout << "PASS CAL-005202\n";
    return 0;
}
