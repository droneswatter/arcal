#pragma once

#include "uci/base/AbstractServiceBusConnection.h"

#include <dds/dds.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace arcal {
namespace dds {

class DdsAbstractServiceBusConnection : public uci::base::AbstractServiceBusConnection {
public:
    explicit DdsAbstractServiceBusConnection(const std::string& serviceLabel);
    ~DdsAbstractServiceBusConnection() override;

    void shutdown() override;

    std::string getMySystemLabel() const override;
    uci::base::UUID getMySystemUUID() const override;
    uci::base::UUID getMyServiceUUID() const override;
    uci::base::UUID getMySubsystemUUID() const override;
    uci::base::UUID getMyComponentUUID(const std::string& name) const override;
    uci::base::UUID getMyCapabilityUUID(const std::string& name) const override;

    std::string getOmsSchemaVersion() const override;
    std::string getOmsSchemaCompilerVersion() const override;
    std::string getOMSApiVersion() const override;
    std::string getAbstractServiceBusConnectionVersion() const override;

    StatusData getStatus() const override;
    void addStatusListener(StatusListener& listener) override;
    void removeStatusListener(StatusListener& listener) override;

    // Expose DDS participant for Reader/Writer construction.
    ::dds::domain::DomainParticipant& participant();

    // Optional: register an Externalizer for application-level message
    // export/import (e.g. saving to files).  Not required for pub/sub transport.
    void registerExternalizer(uci::base::Externalizer& ext) override;

private:
    void transitionState(StateEnum newState, const std::string& detail = "");
    void monitorLoop();

    std::string                         serviceLabel_;
    uci::base::UUID                     systemUUID_;
    uci::base::UUID                     serviceUUID_;
    uci::base::Externalizer*            externalizer_{nullptr};

    ::dds::domain::DomainParticipant    participant_;

    mutable std::mutex                  statusMutex_;
    StatusData                          status_;
    std::vector<StatusListener*>        listeners_;

    std::thread                         monitorThread_;
    std::atomic<bool>                   running_{false};
    std::condition_variable             monitorCv_;
    std::mutex                          monitorCvMutex_;
};

} // namespace dds
} // namespace arcal
