// E2E test — read() respects its timeout.
// Calls read() with a 300ms timeout on a reader with no publisher and verifies
// it returns within a reasonable bound.

#include "uci/type/ActionCommandMT.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include <chrono>
#include <iostream>

struct Listener : public uci::type::ActionCommandMT::Listener {
    void handleMessage(const uci::type::ActionCommandMT&) override {}
};

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("e2e_timeout", "DDS");
    if (!asb) { std::cerr << "failed to get ASB\n"; return 1; }


    auto& reader = uci::type::ActionCommandMT::createReader("TimeoutTopic", asb);

    Listener listener;
    static constexpr unsigned long kTimeoutMs = 300;
    auto t0 = std::chrono::steady_clock::now();
    reader.read(kTimeoutMs, 1, listener);
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    reader.close();
    uci::type::ActionCommandMT::destroyReader(reader);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    // Must not return too early or hang well past the timeout.
    static constexpr long kMaxMs = 3000;
    if (elapsedMs >= static_cast<long>(kTimeoutMs) && elapsedMs < kMaxMs) {
        std::cout << "elapsed=" << elapsedMs << "ms — PASS\n";
        return 0;
    }
    std::cerr << "elapsed=" << elapsedMs << "ms (expected [" << kTimeoutMs
              << ", " << kMaxMs << ")) — FAIL\n";
    return 1;
}
