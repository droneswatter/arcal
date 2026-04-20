// E2E test — two readers, one writer.
// Verifies that each reader independently receives a copy of every published
// message on the same topic.

#include "uci/type/ActionCommandMT.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include <chrono>
#include <iostream>
#include <thread>

struct Listener : public uci::type::ActionCommandMT::Listener {
    int received{0};
    void handleMessage(const uci::type::ActionCommandMT&) override { ++received; }
};

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("e2e_1w2r", "DDS");
    if (!asb) { std::cerr << "failed to get ASB\n"; return 1; }


    auto& readerA = uci::type::ActionCommandMT::createReader("BroadcastTopic", asb);
    auto& readerB = uci::type::ActionCommandMT::createReader("BroadcastTopic", asb);
    auto& writer  = uci::type::ActionCommandMT::createWriter("BroadcastTopic", asb);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    uci::type::ActionCommandMT msg;
    writer.write(msg);

    Listener listenerA, listenerB;
    readerA.read(2000, 1, listenerA);
    readerB.read(2000, 1, listenerB);

    writer.close();
    uci::type::ActionCommandMT::destroyWriter(writer);
    readerA.close();
    uci::type::ActionCommandMT::destroyReader(readerA);
    readerB.close();
    uci::type::ActionCommandMT::destroyReader(readerB);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    if (listenerA.received == 1 && listenerB.received == 1) {
        std::cout << "readerA=" << listenerA.received
                  << " readerB=" << listenerB.received << " — PASS\n";
        return 0;
    }
    std::cerr << "readerA=" << listenerA.received
              << " readerB=" << listenerB.received
              << " (expected 1 and 1) — FAIL\n";
    return 1;
}
