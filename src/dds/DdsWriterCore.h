#pragma once

// DdsWriterCore.h intentionally contains no DDS includes.
// See DdsReaderCore.h for the rationale.

#include "CalQos.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace arcal {
namespace dds {

class DdsAbstractServiceBusConnection; // forward declare

class DdsWriterCore {
public:
    DdsWriterCore(DdsAbstractServiceBusConnection& asb,
                  const std::string& topicName,
                  const CalQos& qos = {});
    ~DdsWriterCore();

    DdsWriterCore(const DdsWriterCore&)            = delete;
    DdsWriterCore& operator=(const DdsWriterCore&) = delete;

    void write(const std::vector<uint8_t>& bytes);
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dds
} // namespace arcal
