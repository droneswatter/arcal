#pragma once

// DdsWriter.h contains no DDS includes.  All DDS logic lives in
// DdsWriterCore.cpp, so changes there never force factories_all.cpp to
// recompile.

#include "DdsWriterCore.h"
#include "arcal/CdrBridge.h"
#include "uci/base/UCIException.h"

namespace arcal {
namespace dds {

class DdsAbstractServiceBusConnection;

template <typename MsgType>
class DdsWriter : public MsgType::Writer {
public:
    DdsWriter(DdsAbstractServiceBusConnection& asb,
              const std::string& topicName,
              const CalQos& qos = {})
        : core_(asb, topicName, qos)
    {}

    ~DdsWriter() override { close(); }

    void write(MsgType& accessor) override {
        if (closed_) throwUciException("DdsWriter: write on closed writer");
        std::vector<uint8_t> bytes;
        arcal::cdrSerialize(accessor, bytes);
        core_.write(arcal::cdrTypeTag(accessor), bytes);
    }

    void close() override {
        closed_ = true;
        core_.close();
    }

private:
    DdsWriterCore core_;
    bool          closed_{false};
};

} // namespace dds
} // namespace arcal
