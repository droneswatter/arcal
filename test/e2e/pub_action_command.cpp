// E2E smoke test — publisher side.
// Publishes one ActionCommandMT on the "ActionCommand" topic and exits 0.
// Intended to run ~1 second after the subscriber starts so DDS peer discovery
// has completed.  Run via run_e2e_action_command.sh or manually:
//
//   ./e2e_sub &; sleep 1; ./e2e_pub

#include "uci/type/ActionCommandMT.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("e2e_pub", "DDS");
    if (!asb) { std::cerr << "pub: failed to get ASB\n"; return 1; }


    auto& writer = uci::type::ActionCommandMT::createWriter("ActionCommand", asb);

    // Wait for DDS peer discovery (SPDP) before writing.
    // With BestEffort QoS, messages sent before discovery completes are lost.
    // Loopback unicast discovery completes in <10ms; 300ms is generous margin.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    uci::type::ActionCommandMT msg;
    writer.write(msg);

    // Give DDS time to deliver before tearing down the participant.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    writer.close();
    uci::type::ActionCommandMT::destroyWriter(writer);

    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    std::cout << "pub: sent one ActionCommandMT\n";
    return 0;
}
