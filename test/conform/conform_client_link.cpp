// Link-only conformance smoke test: representative C++ CAL client surface.
//
// This target is intentionally built but not registered with CTest. It proves
// that a spec-guided C++ CAL application can compile and link using public
// ARCAL headers and libraries without requiring a usable DDS runtime.

#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/AbstractServiceBusConnectionStatusListener.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/type/ServiceStatusMT.h"

#include <string>
#include <vector>

namespace {

class StatusListener final : public uci::base::AbstractServiceBusConnectionStatusListener {
public:
    void statusChanged(uci::base::AbstractServiceBusConnection::AbstractServiceBusConnectionStatusData status) override {
        lastStatus = status;
    }

    uci::base::AbstractServiceBusConnection::AbstractServiceBusConnectionStatusData lastStatus;
};

class ServiceStatusListener final : public uci::type::ServiceStatusMT::Listener {
public:
    void handleMessage(const uci::type::ServiceStatusMT& message) override {
        lastType = message.typeName();
    }

    std::string lastType;
};

void exercise_spec_api_surface()
{
    auto* asb = uci_getAbstractServiceBusConnection(
        std::string{"ConformClient"}, std::string{"DDS"});

    StatusListener statusListener;
    asb->addStatusListener(statusListener);
    (void)asb->getStatus();
    (void)asb->getMySystemLabel();
    (void)asb->getMySystemUUID();
    (void)asb->getMyServiceUUID();
    (void)asb->getMySubsystemUUID();
    (void)asb->getMyComponentUUID("Component");
    (void)asb->getMyCapabilityUUID("Capability");
    (void)asb->getOmsSchemaVersion();
    (void)asb->getOmsSchemaCompilerVersion();
    (void)asb->getOMSApiVersion();
    (void)asb->getAbstractServiceBusConnectionVersion();

    auto& reader = uci::type::ServiceStatusMT::createReader("ServiceStatus", asb);
    auto& writer = uci::type::ServiceStatusMT::createWriter("ServiceStatus", asb);

    ServiceStatusListener actionListener;
    reader.addListener(actionListener);
    (void)reader.readNoWait(1, actionListener);

    auto& message = uci::type::ServiceStatusMT::create(asb);
    writer.write(message);

    reader.removeListener(actionListener);
    writer.close();
    reader.close();
    uci::type::ServiceStatusMT::destroyWriter(writer);
    uci::type::ServiceStatusMT::destroyReader(reader);

    asb->removeStatusListener(statusListener);
    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);

    auto* loader = uci_getExternalizerLoader();
    auto* externalizer = loader->getExternalizer("CDR", "2.5.0", "2.5.0");
    std::vector<uint8_t> bytes;
    externalizer->write(message, bytes);
    externalizer->read(bytes, message);
    loader->destroyExternalizer(externalizer);
    uci_destroyExternalizerLoader(loader);

    uci::type::ServiceStatusMT::destroy(message);
}

} // namespace

int main(int argc, char**)
{
    // Keep the calls link-visible without executing DDS-dependent paths.
    volatile bool run = (argc == -1);
    if (run) {
        exercise_spec_api_surface();
    }
    return 0;
}
