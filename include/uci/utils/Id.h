#pragma once

#include "uci/base/UUID.h"
#include "uci/type/ID_Type.h"

#include <string>

namespace uci {
namespace utils {

inline void setId(uci::type::ID_Type& id, const uci::base::UUID& uuid) {
    id.setUUID(uuid);
}

inline void setId(uci::type::ID_Type& id, const uci::base::UUID& uuid, const std::string& label) {
    setId(id, uuid);
    id.enableDescriptiveLabel() = label;
}

inline void setId(uci::type::ID_Type& id, const std::string& uuid) {
    setId(id, uci::base::UUID::fromString(uuid));
}

inline void setId(uci::type::ID_Type& id, const std::string& uuid, const std::string& label) {
    setId(id, uci::base::UUID::fromString(uuid), label);
}

inline uci::base::UUID assignNewId(uci::type::ID_Type& id) {
    const auto uuid = uci::base::UUID::generateUUID();
    id.setUUID(uuid);
    return uuid;
}

inline uci::base::UUID assignNewId(uci::type::ID_Type& id, const std::string& label) {
    const auto uuid = uci::base::UUID::generateUUID();
    id.setUUID(uuid);
    id.enableDescriptiveLabel() = label;
    return uuid;
}

} // namespace utils
} // namespace uci
