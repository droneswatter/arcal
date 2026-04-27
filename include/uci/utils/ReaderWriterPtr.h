#pragma once

#include "uci/utils/ConnectionPtr.h"
#include "uci/base/AbstractServiceBusConnection.h"

#include <string>

namespace uci {
namespace utils {

template <typename MT>
class ReaderPtr {
public:
    using Reader = typename MT::Reader;

    ReaderPtr(const std::string& topic, uci::base::AbstractServiceBusConnection* asb)
        : reader_(&MT::createReader(topic, asb)) {}

    explicit ReaderPtr(Reader& reader) noexcept : reader_(&reader) {}

    ~ReaderPtr() { resetNoThrow(); }

    ReaderPtr(const ReaderPtr&) = delete;
    ReaderPtr& operator=(const ReaderPtr&) = delete;

    ReaderPtr(ReaderPtr&& rhs) noexcept : reader_(rhs.release()) {}

    ReaderPtr& operator=(ReaderPtr&& rhs) {
        if (this != &rhs) {
            reset();
            reader_ = rhs.release();
        }
        return *this;
    }

    Reader& get() const noexcept { return *reader_; }
    Reader* operator->() const noexcept { return reader_; }
    Reader& operator*() const noexcept { return *reader_; }
    explicit operator bool() const noexcept { return reader_ != nullptr; }

    Reader* release() noexcept {
        auto* released = reader_;
        reader_ = nullptr;
        return released;
    }

    void reset(Reader* reader = nullptr) {
        if (reader_ != nullptr) {
            auto* old = reader_;
            reader_ = nullptr;
            old->close();
            MT::destroyReader(*old);
        }
        reader_ = reader;
    }

private:
    void resetNoThrow() noexcept {
        try {
            reset();
        } catch (...) {
        }
    }

    Reader* reader_{nullptr};
};

template <typename MT>
class WriterPtr {
public:
    using Writer = typename MT::Writer;

    WriterPtr(const std::string& topic, uci::base::AbstractServiceBusConnection* asb)
        : writer_(&MT::createWriter(topic, asb)) {}

    explicit WriterPtr(Writer& writer) noexcept : writer_(&writer) {}

    ~WriterPtr() { resetNoThrow(); }

    WriterPtr(const WriterPtr&) = delete;
    WriterPtr& operator=(const WriterPtr&) = delete;

    WriterPtr(WriterPtr&& rhs) noexcept : writer_(rhs.release()) {}

    WriterPtr& operator=(WriterPtr&& rhs) {
        if (this != &rhs) {
            reset();
            writer_ = rhs.release();
        }
        return *this;
    }

    Writer& get() const noexcept { return *writer_; }
    Writer* operator->() const noexcept { return writer_; }
    Writer& operator*() const noexcept { return *writer_; }
    explicit operator bool() const noexcept { return writer_ != nullptr; }

    Writer* release() noexcept {
        auto* released = writer_;
        writer_ = nullptr;
        return released;
    }

    void reset(Writer* writer = nullptr) {
        if (writer_ != nullptr) {
            auto* old = writer_;
            writer_ = nullptr;
            old->close();
            MT::destroyWriter(*old);
        }
        writer_ = writer;
    }

private:
    void resetNoThrow() noexcept {
        try {
            reset();
        } catch (...) {
        }
    }

    Writer* writer_{nullptr};
};

template <typename MT>
ReaderPtr<MT> makeReader(const std::string& topic,
                         uci::base::AbstractServiceBusConnection* asb) {
    return ReaderPtr<MT>(topic, asb);
}

template <typename MT>
ReaderPtr<MT> makeReader(const std::string& topic,
                         const ConnectionPtr& asb) {
    return makeReader<MT>(topic, asb.get());
}

template <typename MT>
WriterPtr<MT> makeWriter(const std::string& topic,
                         uci::base::AbstractServiceBusConnection* asb) {
    return WriterPtr<MT>(topic, asb);
}

template <typename MT>
WriterPtr<MT> makeWriter(const std::string& topic,
                         const ConnectionPtr& asb) {
    return makeWriter<MT>(topic, asb.get());
}

} // namespace utils
} // namespace uci
