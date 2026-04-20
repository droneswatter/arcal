// E2E test — topic isolation.
// Creates two readers on different topic names and one writer on the first.
// Verifies that only the matching reader receives the message.

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
    auto* asb = uci_getAbstractServiceBusConnection("e2e_isolation", "DDS");
    if (!asb) { std::cerr << "failed to get ASB\n"; return 1; }


    auto& readerA = uci::type::ActionCommandMT::createReader("TopicAlpha", asb);
    auto& readerB = uci::type::ActionCommandMT::createReader("TopicBeta",  asb);
    auto& writer  = uci::type::ActionCommandMT::createWriter("TopicAlpha", asb);

    // Allow DDS reader/writer matching within the same participant.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    uci::type::ActionCommandMT msg;
    writer.write(msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Listener listenerA, listenerB;
    unsigned long nA = readerA.readNoWait(10, listenerA);
    unsigned long nB = readerB.readNoWait(10, listenerB);

    writer.close();
    uci::type::ActionCommandMT::destroyWriter(writer);
    readerA.close();
    uci::type::ActionCommandMT::destroyReader(readerA);
    readerB.close();
    uci::type::ActionCommandMT::destroyReader(readerB);

    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    if (nA == 1 && nB == 0) {
        std::cout << "TopicAlpha=" << nA << " TopicBeta=" << nB << " — PASS\n";
        return 0;
    }
    std::cerr << "TopicAlpha=" << nA << " TopicBeta=" << nB
              << " (expected 1 and 0) — FAIL\n";
    return 1;
}
