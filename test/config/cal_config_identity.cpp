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
      ],
      "services": [
        { "name": "RootSvc", "uuid": "88888888-8888-4888-8888-888888888888" }
      ]
    },
    {
      "name": "AlternateRig",
      "uuid": "99999999-9999-4999-8999-999999999999",
      "services": [
        { "name": "AltSvc", "uuid": "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa" }
      ]
    }
  ]
})json";
}

void writeDuplicateServiceConfig(const std::string& path) {
    std::ofstream out(path);
    out << R"json({
  "systems": [
    {
      "name": "DupRig",
      "uuid": "11111111-1111-4111-8111-111111111111",
      "services": [
        { "name": "DupSvc", "uuid": "22222222-2222-4222-8222-222222222222" }
      ],
      "subsystems": [
        {
          "name": "Payload",
          "uuid": "33333333-3333-4333-8333-333333333333",
          "services": [
            { "name": "DupSvc", "uuid": "44444444-4444-4444-8444-444444444444" }
          ]
        }
      ]
    }
  ]
})json";
}

template <typename Fn>
bool rejects(Fn fn) {
    try {
        fn();
        return false;
    } catch (const uci::base::UCIException&) {
        return true;
    }
}

bool rejectsUnknownService() {
    return rejects([] {
        AsbPtr unknown(uci_getAbstractServiceBusConnection("MissingService", "DDS"));
    });
}

bool rejectsMissingConfig() {
    unsetenv("ARCAL_CONFIG");
    unsetenv("ARCAL_SYSTEM");
    return rejects([] {
        AsbPtr asb(uci_getAbstractServiceBusConnection("Planner", "DDS"));
    });
}

} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string configPath = argv[1];
    const std::string duplicateConfigPath = configPath + ".duplicate";
    writeConfig(configPath);
    writeDuplicateServiceConfig(duplicateConfigPath);

    assert(rejectsMissingConfig());

    setenv("ARCAL_CONFIG", configPath.c_str(), 1);
    unsetenv("ARCAL_SYSTEM");
    assert(rejects([] {
        AsbPtr asb(uci_getAbstractServiceBusConnection("Planner", "DDS"));
    }));

    setenv("ARCAL_SYSTEM", "IntegrationRig", 1);
    AsbPtr asb(uci_getAbstractServiceBusConnection("Planner", "DDS"));
    assert(asb != nullptr);
    assert(asb->getMySystemLabel() == "IntegrationRig");
    assert(asb->getMySystemUUID() == uci::base::UUID::fromString("11111111-1111-4111-8111-111111111111"));
    assert(asb->getMyServiceUUID() == uci::base::UUID::fromString("55555555-5555-4555-8555-555555555555"));
    assert(asb->getMySubsystemUUID() == uci::base::UUID::fromString("44444444-4444-4444-8444-444444444444"));
    assert(asb->getMyComponentUUID("RadarComponent") == uci::base::UUID::fromString("66666666-6666-4666-8666-666666666666"));
    assert(asb->getMyCapabilityUUID("DetectCapability") == uci::base::UUID::fromString("77777777-7777-4777-8777-777777777777"));
    assert(rejectsUnknownService());

    AsbPtr root(uci_getAbstractServiceBusConnection("RootSvc", "DDS"));
    assert(root->getMyServiceUUID() == uci::base::UUID::fromString("88888888-8888-4888-8888-888888888888"));
    assert(!root->getMySubsystemUUID().isValid());

    setenv("ARCAL_SYSTEM", "AlternateRig", 1);
    AsbPtr alternate(uci_getAbstractServiceBusConnection("AltSvc", "DDS"));
    assert(alternate->getMySystemUUID() == uci::base::UUID::fromString("99999999-9999-4999-8999-999999999999"));
    assert(alternate->getMyServiceUUID() == uci::base::UUID::fromString("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"));

    setenv("ARCAL_SYSTEM", "NoSuchRig", 1);
    assert(rejects([] {
        AsbPtr asb(uci_getAbstractServiceBusConnection("Planner", "DDS"));
    }));

    setenv("ARCAL_CONFIG", (configPath + ".missing").c_str(), 1);
    unsetenv("ARCAL_SYSTEM");
    assert(rejects([] {
        AsbPtr asb(uci_getAbstractServiceBusConnection("Planner", "DDS"));
    }));

    setenv("ARCAL_CONFIG", duplicateConfigPath.c_str(), 1);
    unsetenv("ARCAL_SYSTEM");
    assert(rejects([] {
        AsbPtr asb(uci_getAbstractServiceBusConnection("DupSvc", "DDS"));
    }));

    setenv("ARCAL_CONFIG", "NONE", 1);
    unsetenv("ARCAL_SYSTEM");
    AsbPtr fallback(uci_getAbstractServiceBusConnection("FallbackService", "DDS"));
    assert(fallback != nullptr);
    assert(fallback->getMySystemUUID().isValid());
    assert(fallback->getMyServiceUUID().isValid());
    assert(fallback->getMySubsystemUUID().isValid());

    std::cout << "PASS CONFIG-identity\n";
    return 0;
}
