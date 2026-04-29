#pragma once

#include "AbstractServiceBusConnection.h"

namespace uci {
namespace base {

class AbstractServiceBusConnectionStatusListener {
public:
    virtual void statusChanged(
        AbstractServiceBusConnection::AbstractServiceBusConnectionStatusData newStatus) = 0;

    virtual ~AbstractServiceBusConnectionStatusListener() = default;

protected:
    AbstractServiceBusConnectionStatusListener() = default;
    AbstractServiceBusConnectionStatusListener(const AbstractServiceBusConnectionStatusListener&) = default;
    AbstractServiceBusConnectionStatusListener& operator=(const AbstractServiceBusConnectionStatusListener&) = default;
};

} // namespace base
} // namespace uci
