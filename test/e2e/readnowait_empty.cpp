// E2E test — readNoWait returns 0 immediately when no messages are available.

#include "uci/type/ActionCommandMT.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include <iostream>

struct Listener : public uci::type::ActionCommandMT::Listener {
    int received{0};
    void handleMessage(const uci::type::ActionCommandMT&) override { ++received; }
};

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("e2e_empty", "DDS");
    if (!asb) { std::cerr << "failed to get ASB\n"; return 1; }


    auto& reader = uci::type::ActionCommandMT::createReader("EmptyTopic", asb);

    Listener listener;
    unsigned long n = reader.readNoWait(10, listener);

    reader.close();
    uci::type::ActionCommandMT::destroyReader(reader);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    if (n == 0) {
        std::cout << "readNoWait returned " << n << " — PASS\n";
        return 0;
    }
    std::cerr << "readNoWait returned " << n << " (expected 0) — FAIL\n";
    return 1;
}
