#pragma once
#include <cstdint>

namespace arcal {
namespace dds {

struct CalQos {
    uint64_t timeBasedFilterMs{0};

    enum class Reliability { BestEffort, Reliable, Ordering };
    Reliability reliability{Reliability::BestEffort};

    uint64_t expirationMs{0};
    uint32_t messageBufferDepth{1};
};

} // namespace dds
} // namespace arcal
