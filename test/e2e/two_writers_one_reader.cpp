// E2E test — two writers, one reader.
// Verifies that a reader receives messages from two independent writers
// on the same topic.

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
    auto* asb = uci_getAbstractServiceBusConnection("e2e_2w1r", "DDS");
    if (!asb) { std::cerr << "failed to get ASB\n"; return 1; }


    auto& reader   = uci::type::ActionCommandMT::createReader("SharedTopic", asb);
    auto& writerA  = uci::type::ActionCommandMT::createWriter("SharedTopic", asb);
    auto& writerB  = uci::type::ActionCommandMT::createWriter("SharedTopic", asb);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Write and read one at a time: default QoS is KeepLast(1), so sending
    // from both writers without reading would leave only the latest sample.
    Listener listener;
    uci::type::ActionCommandMT msg;
    writerA.write(msg);
    reader.read(2000, 1, listener);
    writerB.write(msg);
    reader.read(2000, 1, listener);

    writerA.close();
    uci::type::ActionCommandMT::destroyWriter(writerA);
    writerB.close();
    uci::type::ActionCommandMT::destroyWriter(writerB);
    reader.close();
    uci::type::ActionCommandMT::destroyReader(reader);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    if (listener.received == 2) {
        std::cout << "received " << listener.received << "/2 — PASS\n";
        return 0;
    }
    std::cerr << "received " << listener.received << "/2 — FAIL\n";
    return 1;
}
