#pragma once

#include "AbstractServiceBusConnectionStatusListener.h"
#include "UUID.h"
#include <string>

namespace uci { namespace base { class Externalizer; } }

namespace uci {
namespace base {

class AbstractServiceBusConnection {
public:
    using StatusData     = AbstractServiceBusConnectionStatusData;
    using StatusListener = AbstractServiceBusConnectionStatusListener;
    using StateEnum      = AbstractServiceBusConnectionStatusData::StateEnum;

    virtual void shutdown() = 0;

    virtual std::string getMySystemLabel() const = 0;
    virtual UUID getMySystemUUID() const = 0;
    virtual UUID getMyServiceUUID() const = 0;
    virtual UUID getMySubsystemUUID() const = 0;
    virtual UUID getMyComponentUUID(const std::string& name) const = 0;
    virtual UUID getMyCapabilityUUID(const std::string& name) const = 0;

    virtual std::string getOmsSchemaVersion() const = 0;
    virtual std::string getOmsSchemaCompilerVersion() const = 0;
    virtual std::string getOMSApiVersion() const = 0;
    virtual std::string getAbstractServiceBusConnectionVersion() const = 0;

    virtual StatusData getStatus() const = 0;
    virtual void addStatusListener(StatusListener& listener) = 0;
    virtual void removeStatusListener(StatusListener& listener) = 0;

    virtual void registerExternalizer(Externalizer& externalizer) = 0;

protected:
    virtual ~AbstractServiceBusConnection() = default;

    AbstractServiceBusConnection() = default;
    AbstractServiceBusConnection(const AbstractServiceBusConnection&) = default;
    AbstractServiceBusConnection& operator=(const AbstractServiceBusConnection&) = default;
};

} // namespace base
} // namespace uci

extern "C"
uci::base::AbstractServiceBusConnection* uci_getAbstractServiceBusConnection(
    const std::string& serviceIdentifier,
    const std::string& typeOfAbstractServiceBusConnection);

extern "C"
void uci_destroyAbstractServiceBusConnection(uci::base::AbstractServiceBusConnection* asb);
