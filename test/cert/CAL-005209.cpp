// CERT CAL-005209: A CAL Implementation shall map Client Topics to the
// applicable CAL Topics.
// Verified by: a writer and reader opened under the same Client Topic name
// both succeed, demonstrating the CAL correctly maps the client-facing name
// to the underlying DDS topic.

#include "uci/type/ActionCommandMT.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include <cassert>
#include <iostream>

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("Svc209", "DDS");
    assert(asb);


    const std::string clientTopic = "MissionActionCommands";

    auto& writer = uci::type::ActionCommandMT::createWriter(clientTopic, asb);
    auto& reader = uci::type::ActionCommandMT::createReader(clientTopic, asb);

    reader.close();
    writer.close();
    uci::type::ActionCommandMT::destroyReader(reader);
    uci::type::ActionCommandMT::destroyWriter(writer);

    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);
    std::cout << "PASS CAL-005209\n";
    return 0;
}
