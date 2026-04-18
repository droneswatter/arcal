#include "DdsAbstractServiceBusConnection.h"
#include "uci/base/UCIException.h"

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace arcal {
namespace dds {

// Version constants
static constexpr const char* OMS_SCHEMA_VERSION      = "2.5.0";
static constexpr const char* SCHEMA_COMPILER_VERSION = "0.1.0";
static constexpr const char* OMS_API_VERSION         = "2.5.0";
static constexpr const char* ASB_VERSION             = "0.1.0";

static constexpr int DEFAULT_DOMAIN_ID = 0;

DdsAbstractServiceBusConnection::DdsAbstractServiceBusConnection(const std::string& serviceLabel)
    : serviceLabel_(serviceLabel)
    , participant_(::dds::domain::DomainParticipant(DEFAULT_DOMAIN_ID))
{
    transitionState(StateEnum::INITIALIZING, "Starting up");

    // Deterministic UUIDs from service label
    systemUUID_  = uci::base::UUID::createVersion3UUID(uci::base::UUID::getNamespaceUUID(), "system");
    serviceUUID_ = uci::base::UUID::createVersion3UUID(uci::base::UUID::getNamespaceUUID(), serviceLabel_);

    transitionState(StateEnum::NORMAL, "Ready");

    running_ = true;
    monitorThread_ = std::thread(&DdsAbstractServiceBusConnection::monitorLoop, this);
}

DdsAbstractServiceBusConnection::~DdsAbstractServiceBusConnection() {
    running_ = false;
    if (monitorThread_.joinable()) monitorThread_.join();
}

void DdsAbstractServiceBusConnection::shutdown() {
    transitionState(StateEnum::FAILED, "Shutdown requested");
    {
        std::lock_guard<std::mutex> lk(monitorCvMutex_);
        running_ = false;
    }
    monitorCv_.notify_all();
    if (monitorThread_.joinable()) monitorThread_.join();
    participant_.close();
}

std::string DdsAbstractServiceBusConnection::getMySystemLabel() const { return serviceLabel_; }
uci::base::UUID DdsAbstractServiceBusConnection::getMySystemUUID()   const { return systemUUID_; }
uci::base::UUID DdsAbstractServiceBusConnection::getMyServiceUUID()  const { return serviceUUID_; }

uci::base::UUID DdsAbstractServiceBusConnection::getMySubsystemUUID() const {
    return uci::base::UUID::createVersion3UUID(serviceUUID_, "subsystem");
}

uci::base::UUID DdsAbstractServiceBusConnection::getMyComponentUUID(const std::string& name) const {
    return uci::base::UUID::createVersion3UUID(serviceUUID_, "component:" + name);
}

uci::base::UUID DdsAbstractServiceBusConnection::getMyCapabilityUUID(const std::string& name) const {
    return uci::base::UUID::createVersion3UUID(serviceUUID_, "capability:" + name);
}

std::string DdsAbstractServiceBusConnection::getOmsSchemaVersion()             const { return OMS_SCHEMA_VERSION; }
std::string DdsAbstractServiceBusConnection::getOmsSchemaCompilerVersion()     const { return SCHEMA_COMPILER_VERSION; }
std::string DdsAbstractServiceBusConnection::getOMSApiVersion()                const { return OMS_API_VERSION; }
std::string DdsAbstractServiceBusConnection::getAbstractServiceBusConnectionVersion() const { return ASB_VERSION; }

auto DdsAbstractServiceBusConnection::getStatus() const -> StatusData {
    std::lock_guard<std::mutex> lock(statusMutex_);
    return status_;
}

void DdsAbstractServiceBusConnection::addStatusListener(StatusListener* listener) {
    if (!listener) return;
    StatusData current;
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        listeners_.push_back(listener);
        current = status_;
    }
    // CERT CAL-016366: call immediately with current state
    listener->statusChanged(current);
}

void DdsAbstractServiceBusConnection::removeStatusListener(StatusListener* listener) {
    std::lock_guard<std::mutex> lock(statusMutex_);
    listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());
}

::dds::domain::DomainParticipant& DdsAbstractServiceBusConnection::participant() {
    return participant_;
}

void DdsAbstractServiceBusConnection::registerExternalizer(uci::base::Externalizer& ext) {
    externalizer_ = &ext;
}

void DdsAbstractServiceBusConnection::transitionState(StateEnum newState, const std::string& detail) {
    std::vector<StatusListener*> snapshot;
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        if (status_.state == newState) return;
        status_.state  = newState;
        status_.detail = detail;
        snapshot = listeners_;
    }
    for (auto* l : snapshot) l->statusChanged({newState, detail});
}

void DdsAbstractServiceBusConnection::monitorLoop() {
    // Periodically check DDS participant liveliness; transition state on failures.
    // Uses a condition variable so shutdown() wakes the thread immediately.
    while (running_) {
        {
            std::unique_lock<std::mutex> lk(monitorCvMutex_);
            monitorCv_.wait_for(lk, std::chrono::seconds(2),
                                [this] { return !running_.load(); });
        }
        if (!running_) break;
        try {
            (void)participant_.domain_id();
        } catch (...) {
            transitionState(StateEnum::INOPERABLE, "DDS participant error");
        }
    }
}

} // namespace dds
} // namespace arcal

// Free function entry point
extern "C" {

uci::base::AbstractServiceBusConnection* uci_getAbstractServiceBusConnection(const char* serviceLabel) {
    return new arcal::dds::DdsAbstractServiceBusConnection(serviceLabel ? serviceLabel : "default");
}

void uci_destroyAbstractServiceBusConnection(uci::base::AbstractServiceBusConnection* asb) {
    delete asb;
}

} // extern "C"
