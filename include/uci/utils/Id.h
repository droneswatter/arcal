#pragma once

#include "uci/base/UUID.h"
#include "uci/type/ID_Type.h"

#include <string>

namespace uci {
namespace utils {

inline uci::base::UUID newUuid() {
    return uci::base::UUID::generateUUID();
}

inline void setId(uci::type::ID_Type& id, const uci::base::UUID& uuid) {
    id.setUUID(uuid);
}

inline void setId(uci::type::ID_Type& id, const uci::base::UUID& uuid, const std::string& label) {
    setId(id, uuid);
    id.enableDescriptiveLabel().setValue(label);
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
    id.enableDescriptiveLabel().setValue(label);
    return uuid;
}

inline uci::base::UUID uuidOf(const uci::type::ID_Type& id) {
    return id.getUUID();
}

inline std::string uuidStringOf(const uci::type::ID_Type& id) {
    return uuidOf(id).toString();
}

inline std::string labelOf(const uci::type::ID_Type& id) {
    return id.hasDescriptiveLabel() ? id.getDescriptiveLabel().getValue() : std::string{};
}

} // namespace utils
} // namespace uci
