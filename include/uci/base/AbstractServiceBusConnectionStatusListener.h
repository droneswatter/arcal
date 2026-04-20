#pragma once

#include <string>

namespace uci {
namespace base {

struct AbstractServiceBusConnectionStatusData {
    enum StateEnum {
        INITIALIZING,
        NORMAL,
        DEGRADED,
        INOPERABLE,
        FAILED
    };

    StateEnum   state{INITIALIZING};
    std::string stateDetail;
};

class AbstractServiceBusConnectionStatusListener {
public:
    virtual void statusChanged(AbstractServiceBusConnectionStatusData newStatus) = 0;

    virtual ~AbstractServiceBusConnectionStatusListener() = default;

protected:
    AbstractServiceBusConnectionStatusListener() = default;
    AbstractServiceBusConnectionStatusListener(const AbstractServiceBusConnectionStatusListener&) = default;
    AbstractServiceBusConnectionStatusListener& operator=(const AbstractServiceBusConnectionStatusListener&) = default;
};

} // namespace base
} // namespace uci
