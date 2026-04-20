// E2E test — addListener / removeListener lifecycle.
// Verifies that a removed listener stops receiving messages.

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
    auto* asb = uci_getAbstractServiceBusConnection("e2e_listener", "DDS");
    if (!asb) { std::cerr << "failed to get ASB\n"; return 1; }


    auto& reader = uci::type::ActionCommandMT::createReader("ListenerTopic", asb);
    auto& writer = uci::type::ActionCommandMT::createWriter("ListenerTopic", asb);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    Listener listener;
    reader.addListener(listener);

    // Publish and wait for background delivery.
    uci::type::ActionCommandMT msg;
    writer.write(msg);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    int afterFirst = listener.received;

    reader.removeListener(listener);

    // Publish again — removed listener must not receive this.
    writer.write(msg);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    int afterSecond = listener.received;

    writer.close();
    uci::type::ActionCommandMT::destroyWriter(writer);
    reader.close();
    uci::type::ActionCommandMT::destroyReader(reader);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    if (afterFirst == 1 && afterSecond == 1) {
        std::cout << "after-add=" << afterFirst << " after-remove=" << afterSecond << " — PASS\n";
        return 0;
    }
    std::cerr << "after-add=" << afterFirst << " after-remove=" << afterSecond
              << " (expected 1 and 1) — FAIL\n";
    return 1;
}
