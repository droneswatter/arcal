#pragma once

// DdsReaderCore.h intentionally contains no DDS includes.  All DDS headers
// are confined to DdsReaderCore.cpp via the PIMPL idiom so that changes to
// WaitSet / threading logic there do not force factories_all.cpp to recompile.

#include "CalQos.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace arcal {
namespace dds {

class DdsAbstractServiceBusConnection; // forward declare — full type in .cpp only

class DdsReaderCore {
public:
    struct TaggedSample {
        uint32_t             tag;
        std::vector<uint8_t> data;
    };
    using SamplesCallback = std::function<void(std::vector<TaggedSample>)>;

    DdsReaderCore(DdsAbstractServiceBusConnection& asb,
                  const std::string& topicName,
                  const CalQos& qos = {});
    ~DdsReaderCore();

    DdsReaderCore(const DdsReaderCore&)            = delete;
    DdsReaderCore& operator=(const DdsReaderCore&) = delete;

    // Block up to timeoutMs ms; return up to maxSamples raw payloads (0 = no cap).
    std::vector<TaggedSample> waitAndTake(unsigned long timeoutMs,
                                          unsigned long maxSamples);

    // Non-blocking take; return up to maxSamples raw payloads (0 = no cap).
    std::vector<TaggedSample> takeNow(unsigned long maxSamples);

    // Register callback for async background delivery when persistent listeners
    // are registered.  Pass nullptr to put the background thread to sleep.
    void setBackgroundCallback(SamplesCallback cb);

    // Signal the core as closed; no further WaitSet waits will be initiated.
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dds
} // namespace arcal
