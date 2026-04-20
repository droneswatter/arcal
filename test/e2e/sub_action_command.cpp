// E2E smoke test — subscriber side.
// Waits up to 5 seconds for one ActionCommandMT on the "ActionCommand" topic.
// Exits 0 on receipt, 1 on timeout.
// Intended to start before the publisher.  Run via run_e2e_action_command.sh
// or manually:
//
//   ./e2e_sub &; sleep 1; ./e2e_pub; wait

#include "uci/type/ActionCommandMT.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include <iostream>

struct ActionCommandListener : public uci::type::ActionCommandMT::Listener {
    int received{0};
    void handleMessage(const uci::type::ActionCommandMT&) override { ++received; }
};

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("e2e_sub", "DDS");
    if (!asb) { std::cerr << "sub: failed to get ASB\n"; return 1; }


    auto& reader = uci::type::ActionCommandMT::createReader("ActionCommand", asb);

    ActionCommandListener listener;
    // Block up to 2 seconds for one message.
    reader.read(2000, 1, listener);

    reader.close();
    uci::type::ActionCommandMT::destroyReader(reader);

    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    if (listener.received > 0) {
        std::cout << "sub: received " << listener.received << " ActionCommandMT message(s) — PASS\n";
        return 0;
    }
    std::cerr << "sub: timed out waiting for ActionCommandMT — FAIL\n";
    return 1;
}
