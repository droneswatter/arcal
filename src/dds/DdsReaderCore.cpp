#include "DdsReaderCore.h"
#include "DdsAbstractServiceBusConnection.h"
#include "QosMapper.h"
#include "arcal_payload.hpp"

#include <dds/dds.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace arcal {
namespace dds {

struct DdsReaderCore::Impl {
    ::dds::topic::Topic<arcal_dds::OpaquePayload>    topic;
    ::dds::sub::DataReader<arcal_dds::OpaquePayload> reader;
    std::atomic<bool>  closed{false};
    std::mutex         cbMutex;
    SamplesCallback    callback;
    std::thread        bgThread;

    Impl(DdsAbstractServiceBusConnection& asb,
         const std::string& topicName, const CalQos& qos)
        : topic(asb.participant(), topicName, ::dds::topic::qos::TopicQos())
        , reader(::dds::sub::Subscriber(asb.participant()), topic, toReaderQos(qos))
    {
        bgThread = std::thread([this] { loop(); });
    }

    ~Impl() {
        closed = true;
        if (bgThread.joinable()) bgThread.join();
    }

    std::vector<SampleBytes> take(unsigned long maxSamples) {
        std::vector<SampleBytes> out;
        auto samples = reader.take();
        for (auto& s : samples) {
            if (!s.info().valid()) continue;
            if (maxSamples > 0 && out.size() >= maxSamples) break;
            out.push_back(s.data().data());
        }
        return out;
    }

    std::vector<SampleBytes> waitAndTake(unsigned long timeoutMs,
                                          unsigned long maxSamples) {
        ::dds::core::cond::WaitSet ws;
        auto rc = ::dds::sub::cond::ReadCondition(
            reader, ::dds::sub::status::DataState::any());
        ws += rc;
        auto dur = ::dds::core::Duration::from_millisecs(
            static_cast<int64_t>(timeoutMs));
        try { ws.wait(dur); } catch (...) {}
        return take(maxSamples);
    }

    void loop() {
        while (!closed) {
            // Check whether there is a callback to deliver to.
            SamplesCallback cb;
            { std::lock_guard<std::mutex> lk(cbMutex); cb = callback; }
            if (!cb) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Wait for data (or 100 ms poll interval).
            try {
                ::dds::core::cond::WaitSet ws;
                auto rc = ::dds::sub::cond::ReadCondition(
                    reader, ::dds::sub::status::DataState::any());
                ws += rc;
                ws.wait(::dds::core::Duration(0, 100'000'000));
            } catch (...) {}

            if (closed) break;

            auto samples = take(0);
            if (samples.empty()) continue;

            // Re-check under the mutex before invoking; the callback may have
            // been cleared (set to nullptr) while we were waiting or taking.
            std::lock_guard<std::mutex> lk(cbMutex);
            if (callback) callback(std::move(samples));
        }
    }
};

DdsReaderCore::DdsReaderCore(DdsAbstractServiceBusConnection& asb,
                              const std::string& topicName, const CalQos& qos)
    : impl_(std::make_unique<Impl>(asb, topicName, qos))
{}

DdsReaderCore::~DdsReaderCore() = default;

std::vector<DdsReaderCore::SampleBytes>
DdsReaderCore::waitAndTake(unsigned long timeoutMs, unsigned long maxSamples) {
    return impl_->waitAndTake(timeoutMs, maxSamples);
}

std::vector<DdsReaderCore::SampleBytes>
DdsReaderCore::takeNow(unsigned long maxSamples) {
    return impl_->take(maxSamples);
}

void DdsReaderCore::setBackgroundCallback(SamplesCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cbMutex);
    impl_->callback = std::move(cb);
}

void DdsReaderCore::close() {
    impl_->closed = true;
}

} // namespace dds
} // namespace arcal
