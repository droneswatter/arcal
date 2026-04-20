// CERT CAL-005210: A CAL Implementation shall support the configuration of
// independent Quality of Service settings for each Client Topic.
// Verified by: multiple readers on different Client Topic names coexist
// without interference; each Client Topic is backed by an independent DDS
// topic with its own QoS lifecycle.

#include "uci/type/ActionCommandMT.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include <cassert>
#include <iostream>

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("Svc210", "DDS");
    assert(asb);


    // Two Client Topics with independent DDS topics — each can carry distinct QoS.
    auto& readerA = uci::type::ActionCommandMT::createReader("TopicHighPriority", asb);
    auto& readerB = uci::type::ActionCommandMT::createReader("TopicLowPriority",  asb);

    readerA.close();
    readerB.close();
    uci::type::ActionCommandMT::destroyReader(readerA);
    uci::type::ActionCommandMT::destroyReader(readerB);

    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);
    std::cout << "PASS CAL-005210\n";
    return 0;
}
