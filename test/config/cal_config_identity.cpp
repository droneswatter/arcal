#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/UCIException.h"

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

struct AsbDeleter {
    void operator()(uci::base::AbstractServiceBusConnection* asb) const {
        if (asb != nullptr) uci_destroyAbstractServiceBusConnection(asb);
    }
};

using AsbPtr = std::unique_ptr<uci::base::AbstractServiceBusConnection, AsbDeleter>;

void writeConfig(const std::string& path) {
    std::ofstream out(path);
    out << R"json({
  "systems": [
    {
      "name": "IntegrationRig",
      "uuid": "11111111-1111-4111-8111-111111111111",
      "components": [
        { "name": "SystemComponent", "uuid": "22222222-2222-4222-8222-222222222222" }
      ],
      "capabilities": [
        { "name": "SystemCapability", "uuid": "33333333-3333-4333-8333-333333333333" }
      ],
      "subsystems": [
        {
          "name": "Payload",
          "uuid": "44444444-4444-4444-8444-444444444444",
          "services": [
            { "name": "Planner", "uuid": "55555555-5555-4555-8555-555555555555" }
          ],
          "components": [
            { "name": "RadarComponent", "uuid": "66666666-6666-4666-8666-666666666666" }
          ],
          "capabilities": [
            { "name": "DetectCapability", "uuid": "77777777-7777-4777-8777-777777777777" }
          ]
        }
      ]
    }
  ]
})json";
}

bool rejectsUnknownService() {
    try {
        AsbPtr unknown(uci_getAbstractServiceBusConnection("MissingService", "DDS"));
        return false;
    } catch (const uci::base::UCIException&) {
        return true;
    }
}

bool rejectsMissingConfig() {
    unsetenv("ARCAL_CONFIG");
    unsetenv("ARCAL_SYSTEM");
    try {
        AsbPtr asb(uci_getAbstractServiceBusConnection("Planner", "DDS"));
        return false;
    } catch (const uci::base::UCIException&) {
        return true;
    }
}

} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string configPath = argv[1];
    writeConfig(configPath);

    assert(rejectsMissingConfig());

    setenv("ARCAL_CONFIG", configPath.c_str(), 1);
    unsetenv("ARCAL_SYSTEM");

    AsbPtr asb(uci_getAbstractServiceBusConnection("Planner", "DDS"));
    assert(asb != nullptr);
    assert(asb->getMySystemLabel() == "IntegrationRig");
    assert(asb->getMySystemUUID() == uci::base::UUID::fromString("11111111-1111-4111-8111-111111111111"));
    assert(asb->getMyServiceUUID() == uci::base::UUID::fromString("55555555-5555-4555-8555-555555555555"));
    assert(asb->getMySubsystemUUID() == uci::base::UUID::fromString("44444444-4444-4444-8444-444444444444"));
    assert(asb->getMyComponentUUID("RadarComponent") == uci::base::UUID::fromString("66666666-6666-4666-8666-666666666666"));
    assert(asb->getMyCapabilityUUID("DetectCapability") == uci::base::UUID::fromString("77777777-7777-4777-8777-777777777777"));
    assert(rejectsUnknownService());

    setenv("ARCAL_CONFIG", "NONE", 1);
    AsbPtr fallback(uci_getAbstractServiceBusConnection("FallbackService", "DDS"));
    assert(fallback != nullptr);
    assert(fallback->getMySystemUUID().isValid());
    assert(fallback->getMyServiceUUID().isValid());
    assert(fallback->getMySubsystemUUID().isValid());

    std::cout << "PASS CONFIG-identity\n";
    return 0;
}
