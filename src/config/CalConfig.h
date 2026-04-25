#pragma once

#include "uci/base/UUID.h"

#include <string>
#include <unordered_map>

namespace arcal {
namespace config {

struct CalIdentity {
    bool configured{false};
    std::string systemLabel;
    std::string serviceLabel;
    uci::base::UUID systemUUID;
    uci::base::UUID serviceUUID;
    uci::base::UUID subsystemUUID;
    std::unordered_map<std::string, uci::base::UUID> componentUUIDs;
    std::unordered_map<std::string, uci::base::UUID> capabilityUUIDs;
};

CalIdentity resolveCalIdentity(const std::string& serviceLabel);

} // namespace config
} // namespace arcal
