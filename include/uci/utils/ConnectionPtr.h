#pragma once

#include "uci/base/AbstractServiceBusConnection.h"

#include <string>

namespace uci {
namespace utils {

class ConnectionPtr {
public:
    ConnectionPtr(const std::string& serviceIdentifier,
                  const std::string& typeOfAbstractServiceBusConnection)
        : asb_(uci_getAbstractServiceBusConnection(serviceIdentifier,
                                                   typeOfAbstractServiceBusConnection)) {}

    explicit ConnectionPtr(uci::base::AbstractServiceBusConnection* asb) noexcept
        : asb_(asb) {}

    ~ConnectionPtr() { resetNoThrow(); }

    ConnectionPtr(const ConnectionPtr&) = delete;
    ConnectionPtr& operator=(const ConnectionPtr&) = delete;

    ConnectionPtr(ConnectionPtr&& rhs) noexcept : asb_(rhs.release()) {}

    ConnectionPtr& operator=(ConnectionPtr&& rhs) {
        if (this != &rhs) {
            reset();
            asb_ = rhs.release();
        }
        return *this;
    }

    uci::base::AbstractServiceBusConnection* get() const noexcept { return asb_; }
    uci::base::AbstractServiceBusConnection* operator->() const noexcept { return asb_; }
    uci::base::AbstractServiceBusConnection& operator*() const noexcept { return *asb_; }
    explicit operator bool() const noexcept { return asb_ != nullptr; }

    uci::base::AbstractServiceBusConnection* release() noexcept {
        auto* released = asb_;
        asb_ = nullptr;
        return released;
    }

    void reset(uci::base::AbstractServiceBusConnection* asb = nullptr) {
        if (asb_ != nullptr) {
            auto* old = asb_;
            asb_ = nullptr;
            old->shutdown();
            uci_destroyAbstractServiceBusConnection(old);
        }
        asb_ = asb;
    }

private:
    void resetNoThrow() noexcept {
        try {
            reset();
        } catch (...) {
        }
    }

    uci::base::AbstractServiceBusConnection* asb_{nullptr};
};

inline ConnectionPtr makeConnection(const std::string& serviceIdentifier,
                                    const std::string& typeOfAbstractServiceBusConnection = "DDS") {
    return ConnectionPtr(serviceIdentifier, typeOfAbstractServiceBusConnection);
}

} // namespace utils
} // namespace uci
