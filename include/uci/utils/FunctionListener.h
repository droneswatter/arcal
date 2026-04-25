#pragma once

#include <functional>
#include <utility>

namespace uci {
namespace utils {

template <typename MT>
class FunctionListener final : public MT::Listener {
public:
    using Message = MT;
    using Callback = std::function<void(const MT&)>;

    explicit FunctionListener(Callback callback)
        : callback_(std::move(callback)) {}

    void handleMessage(const MT& message) override {
        callback_(message);
    }

private:
    Callback callback_;
};

template <typename MT>
class ScopedListener {
public:
    using Reader = typename MT::Reader;
    using Listener = typename MT::Listener;

    ScopedListener(Reader& reader, Listener& listener)
        : reader_(&reader)
        , listener_(&listener) {
        reader_->addListener(*listener_);
    }

    ~ScopedListener() {
        if (reader_ != nullptr && listener_ != nullptr) {
            reader_->removeListener(*listener_);
        }
    }

    ScopedListener(const ScopedListener&) = delete;
    ScopedListener& operator=(const ScopedListener&) = delete;
    ScopedListener(ScopedListener&&) = delete;
    ScopedListener& operator=(ScopedListener&&) = delete;

private:
    Reader* reader_;
    Listener* listener_;
};

template <typename MT, typename Callback>
FunctionListener<MT> makeListener(Callback&& callback) {
    return FunctionListener<MT>(typename FunctionListener<MT>::Callback(std::forward<Callback>(callback)));
}

} // namespace utils
} // namespace uci
