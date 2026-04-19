// Continuous ActionCommandMT publisher.
// Keeps one DDS participant alive and sends at a fixed rate with fields that
// change every message so the web UI shows live variation.
//
// Usage:
//   pub_continuous [--rate MSG/S] [--count N] [--topic NAME]
//
//   --rate   messages per second  (default: 10, max: ~1000)
//   --count  total messages; 0 = run until Ctrl-C  (default: 0)
//   --topic  DDS topic name  (default: ActionCommand)

#include "uci/type/ActionCommandMT.h"
#include "uci/type/ActionCommandType.h"
#include "uci/base/AbstractServiceBusConnection.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <csignal>
#include <sstream>
#include <string>
#include <thread>

static std::atomic<bool> g_running{true};
static void sighandler(int) { g_running = false; }

// Format a wall-clock timestamp as ISO-8601 millisecond string.
static std::string nowIso() {
    using namespace std::chrono;
    auto now   = system_clock::now();
    auto ms    = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = system_clock::to_time_t(now);
    std::tm bt{};
    gmtime_r(&timer, &bt);
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

// Encode a 64-bit counter into a UUID-shaped string so each message has a
// unique, human-readable ActionID.
static std::string seqUuid(uint64_t n) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << (n >> 32 & 0xffffffff) << '-'
        << std::setw(4) << (n >> 16 & 0xffff)     << '-'
        << std::setw(4) << (n       & 0xffff)      << '-'
        << "8000-000000000000";
    return oss.str();
}

int main(int argc, char** argv) {
    int         rate  = 10;
    uint64_t    count = 0;
    std::string topic = "ActionCommand";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--rate")  == 0 && i+1 < argc) rate  = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--count") == 0 && i+1 < argc) count = std::stoull(argv[++i]);
        else if (std::strcmp(argv[i], "--topic") == 0 && i+1 < argc) topic = argv[++i];
        else {
            std::cerr << "usage: pub_continuous [--rate N] [--count N] [--topic NAME]\n";
            return 1;
        }
    }

    std::signal(SIGINT,  sighandler);
    std::signal(SIGTERM, sighandler);

    auto* asb = uci_getAbstractServiceBusConnection("pub_continuous");
    if (!asb) { std::cerr << "pub_continuous: failed to get ASB\n"; return 1; }

    auto& writer = uci::type::ActionCommandMT::createWriter(topic, asb);

    // Wait for DDS peer discovery before the first write.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    const auto period = std::chrono::microseconds(1'000'000 / std::max(rate, 1));
    uint64_t sent = 0;
    auto next = std::chrono::steady_clock::now();

    std::cout << "pub_continuous: topic=" << topic
              << "  rate=" << rate << "/s"
              << (count ? "  count=" + std::to_string(count) : "  count=∞")
              << "\n";

    // Priority cycles 1–10; mode alternates LIVE/EXERCISE so the enum field
    // visibly changes in the detail panel.
    const uci::type::MessageModeEnum::EnumerationItem kModes[] = {
        uci::type::MessageModeEnum::LIVE,
        uci::type::MessageModeEnum::EXERCISE,
        uci::type::MessageModeEnum::SIMULATION,
    };

    while (g_running && (count == 0 || sent < count)) {
        uci::type::ActionCommandMT msg;

        // ── MessageHeader ────────────────────────────────────────────────────
        auto& hdr = msg.getMessageHeader();
        hdr.getSystemID().getUUID().setValue("11111111-1111-1111-1111-111111111111");
        hdr.getSystemID().enableDescriptiveLabel().setValue("SENSOR-PLATFORM-ALPHA");
        hdr.getTimestamp().setValue(nowIso());          // live wall-clock time
        hdr.getSchemaVersion().setValue("2.5.0");
        hdr.getMode().setValue(kModes[sent % 3]);       // rotates LIVE/EXERCISE/SIMULATION
        hdr.enableServiceID().getUUID().setValue("22222222-2222-2222-2222-222222222222");
        hdr.enableMissionID().getUUID().setValue("33333333-3333-3333-3333-333333333333");

        // ── MessageData.Command[0] ───────────────────────────────────────────
        uci::type::ActionCommandType cmdEntry;
        auto& cap = cmdEntry.chooseCapability();

        cap.getCapabilityID().getUUID().setValue("44444444-4444-4444-4444-444444444444");
        cap.getCapabilityID().enableDescriptiveLabel().setValue("LONG-RANGE-TRACK");

        // Priority cycles 1–10 so the Ranking field visibly increments.
        const uint16_t priority = static_cast<uint16_t>((sent % 10) + 1);
        cap.getRanking().getRank().getPriority().setValue(priority);
        cap.getRanking().getRank().enablePrecedenceWithinPriority().setValue(
            static_cast<uint16_t>(sent % 5 + 1));
        cap.getRanking().enableInterruptOtherActivities() = (sent % 2 == 0);

        // ActionID UUID encodes the message counter — unique and traceable.
        cap.getActionID().getUUID().setValue(seqUuid(sent + 1));
        cap.getActionID().enableVersion() = static_cast<uint32_t>((sent / 10) + 1);

        msg.getMessageData().getCommand().push_back(cmdEntry);

        writer.write(msg);
        ++sent;

        // Rate-limit: sleep until the next scheduled send time.
        next += period;
        std::this_thread::sleep_until(next);
    }

    std::cout << "pub_continuous: sent " << sent << " messages\n";

    writer.close();
    uci::type::ActionCommandMT::destroyWriter(writer);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);
    return 0;
}
