#include "DdsWriterCore.h"
#include "DdsAbstractServiceBusConnection.h"
#include "QosMapper.h"
#include "arcal_payload.hpp"

#include <dds/dds.hpp>

#include <atomic>

namespace arcal {
namespace dds {

struct DdsWriterCore::Impl {
    ::dds::topic::Topic<arcal_dds::OpaquePayload>    topic;
    ::dds::pub::DataWriter<arcal_dds::OpaquePayload> writer;
    std::atomic<bool> closed{false};

    Impl(DdsAbstractServiceBusConnection& asb,
         const std::string& topicName, const CalQos& qos)
        : topic(asb.participant(), topicName, ::dds::topic::qos::TopicQos())
        , writer(::dds::pub::Publisher(asb.participant()), topic, toWriterQos(qos))
    {}
};

DdsWriterCore::DdsWriterCore(DdsAbstractServiceBusConnection& asb,
                              const std::string& topicName, const CalQos& qos)
    : impl_(std::make_unique<Impl>(asb, topicName, qos))
{}

DdsWriterCore::~DdsWriterCore() = default;

void DdsWriterCore::write(uint32_t tag, const std::vector<uint8_t>& bytes) {
    if (impl_->closed) return;
    arcal_dds::OpaquePayload payload;
    payload.type_tag(tag);
    payload.data(bytes);
    impl_->writer.write(payload);
}

void DdsWriterCore::close() {
    impl_->closed = true;
}

} // namespace dds
} // namespace arcal
