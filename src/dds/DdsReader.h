#pragma once

// DdsReader.h contains no DDS includes.  All DDS logic lives in
// DdsReaderCore.cpp, so changes there never force factories_all.cpp to
// recompile.

#include "DdsReaderCore.h"
#include "arcal/CdrBridge.h"
#include "uci/base/UCIException.h"

#include <algorithm>
#include <mutex>
#include <vector>

namespace arcal {
namespace dds {

class DdsAbstractServiceBusConnection;

template <typename MsgType>
class DdsReader : public MsgType::Reader {
public:
    using ListenerType = typename MsgType::Listener;

    DdsReader(DdsAbstractServiceBusConnection& asb,
              const std::string& topicName,
              const CalQos& qos = {})
        : core_(asb, topicName, qos)
    {}

    // Null the background callback before any other member is destroyed.
    ~DdsReader() override {
        core_.setBackgroundCallback(nullptr);
    }

    void addListener(ListenerType& listener) override {
        std::lock_guard<std::mutex> lk(mu_);
        listeners_.push_back(&listener);
        if (listeners_.size() == 1)
            core_.setBackgroundCallback(
                [this](auto s) { onBackgroundSamples(std::move(s)); });
    }

    void removeListener(ListenerType& listener) override {
        std::lock_guard<std::mutex> lk(mu_);
        listeners_.erase(
            std::remove(listeners_.begin(), listeners_.end(), &listener),
            listeners_.end());
        if (listeners_.empty())
            core_.setBackgroundCallback(nullptr);
    }

    unsigned long read(unsigned long timeoutMs, unsigned long numberOfMessages,
                       ListenerType& listener) override {
        if (closed_) throwUciException("DdsReader: read on closed reader");
        auto raw = core_.waitAndTake(timeoutMs, numberOfMessages);
        for (auto& s : raw) dispatchOne(s, &listener);
        return static_cast<unsigned long>(raw.size());
    }

    unsigned long readNoWait(unsigned long numberOfMessages,
                             ListenerType& listener) override {
        if (closed_) throwUciException("DdsReader: readNoWait on closed reader");
        auto raw = core_.takeNow(numberOfMessages);
        for (auto& s : raw) dispatchOne(s, &listener);
        return static_cast<unsigned long>(raw.size());
    }

    void close() override {
        closed_ = true;
        core_.setBackgroundCallback(nullptr);
        core_.close();
    }

private:
    void dispatchOne(const DdsReaderCore::TaggedSample& s,
                     ListenerType* callListener) {
        MsgType msg;
        const uint32_t expected = arcal::cdrTypeTag(msg);
        if (s.tag != expected) return; // wrong type on this topic; silently drop
        arcal::cdrDeserialize(s.tag, s.data, msg);
        if (callListener) callListener->handleMessage(msg);
        std::vector<ListenerType*> snap;
        { std::lock_guard<std::mutex> lk(mu_); snap = listeners_; }
        for (auto* l : snap) l->handleMessage(msg);
    }

    void onBackgroundSamples(std::vector<DdsReaderCore::TaggedSample> samples) {
        for (auto& s : samples) dispatchOne(s, nullptr);
    }

    // core_ is declared first so it is destroyed last, ensuring the background
    // thread is joined only after the callback has been nulled (in ~DdsReader).
    DdsReaderCore              core_;
    std::mutex                 mu_;
    std::vector<ListenerType*> listeners_;
    bool                       closed_{false};
};

} // namespace dds
} // namespace arcal
