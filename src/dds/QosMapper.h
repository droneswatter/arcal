#pragma once

// QosMapper.h is intentionally included only from DdsReaderCore.cpp and
// DdsWriterCore.cpp — never from template headers — so the DDS headers it
// pulls in don't inflate the compilation of factories_all.cpp.

#include "CalQos.h"

#include <dds/dds.hpp>
#include <cstdint>

namespace arcal {
namespace dds {

inline ::dds::sub::qos::DataReaderQos toReaderQos(const CalQos& cal) {
    ::dds::sub::qos::DataReaderQos qos;

    if (cal.timeBasedFilterMs > 0) {
        qos << ::dds::core::policy::TimeBasedFilter(
            ::dds::core::Duration::from_millisecs(cal.timeBasedFilterMs));
    }

    if (cal.reliability == CalQos::Reliability::BestEffort) {
        qos << ::dds::core::policy::Reliability::BestEffort();
    } else {
        qos << ::dds::core::policy::Reliability::Reliable();
    }

    // Lifespan is a writer-only policy; expiration on the reader side is
    // enforced by the writer's Lifespan QoS, not set here.

    uint32_t depth = cal.messageBufferDepth > 0 ? cal.messageBufferDepth : ::dds::core::LENGTH_UNLIMITED;
    qos << ::dds::core::policy::History::KeepLast(depth);

    return qos;
}

inline ::dds::pub::qos::DataWriterQos toWriterQos(const CalQos& cal) {
    ::dds::pub::qos::DataWriterQos qos;

    if (cal.reliability == CalQos::Reliability::BestEffort) {
        qos << ::dds::core::policy::Reliability::BestEffort();
    } else {
        qos << ::dds::core::policy::Reliability::Reliable();
    }

    if (cal.expirationMs > 0) {
        qos << ::dds::core::policy::Lifespan(
            ::dds::core::Duration::from_millisecs(cal.expirationMs));
    }

    uint32_t depth = cal.messageBufferDepth > 0 ? cal.messageBufferDepth : ::dds::core::LENGTH_UNLIMITED;
    qos << ::dds::core::policy::History::KeepLast(depth);

    return qos;
}

} // namespace dds
} // namespace arcal
