#pragma once

#include "uci/base/AbstractServiceBusConnection.h"

#include <utility>

namespace uci {
namespace utils {

template <typename T>
class MessagePtr {
public:
    explicit MessagePtr(T& accessor) noexcept : accessor_(&accessor) {}
    ~MessagePtr() { resetNoThrow(); }

    MessagePtr(const MessagePtr&) = delete;
    MessagePtr& operator=(const MessagePtr&) = delete;

    MessagePtr(MessagePtr&& rhs) noexcept : accessor_(rhs.release()) {}

    MessagePtr& operator=(MessagePtr&& rhs) {
        if (this != &rhs) {
            reset();
            accessor_ = rhs.release();
        }
        return *this;
    }

    T& get() const noexcept { return *accessor_; }
    T* operator->() const noexcept { return accessor_; }
    T& operator*() const noexcept { return *accessor_; }
    explicit operator bool() const noexcept { return accessor_ != nullptr; }

    T* release() noexcept {
        T* released = accessor_;
        accessor_ = nullptr;
        return released;
    }

    void reset(T* accessor = nullptr) {
        if (accessor_ != nullptr) {
            auto* old = accessor_;
            accessor_ = nullptr;
            T::destroy(*old);
        }
        accessor_ = accessor;
    }

private:
    void resetNoThrow() noexcept {
        try {
            reset();
        } catch (...) {
        }
    }

    T* accessor_{nullptr};
};

// Named for the common application case, but works for any generated
// Accessor-derived type that exposes the CxxCAL create/destroy lifecycle.
template <typename T>
MessagePtr<T> makeMessage(uci::base::AbstractServiceBusConnection* asb) {
    return MessagePtr<T>(T::create(asb));
}

} // namespace utils
} // namespace uci
