#include "config/CalConfig.h"
#include "uci/base/UCIException.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <unordered_set>

namespace arcal {
namespace config {
namespace {

using json = nlohmann::json;

struct ServiceInfo {
    uci::base::UUID serviceUUID;
    uci::base::UUID subsystemUUID;
};

std::string requireString(const json& object, const char* key, const std::string& context) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        throwUciException("CAL config: " << context << " must contain string '" << key << "'");
    }
    return it->get<std::string>();
}

uci::base::UUID requireUUID(const json& object, const char* key, const std::string& context) {
    try {
        return uci::base::UUID::fromString(requireString(object, key, context));
    } catch (const std::exception& e) {
        throwUciException("CAL config: invalid UUID for " << context << "." << key << ": " << e.what());
    }
}

void requireArrayIfPresent(const json& object, const char* key, const std::string& context) {
    auto it = object.find(key);
    if (it != object.end() && !it->is_array()) {
        throwUciException("CAL config: " << context << "." << key << " must be an array");
    }
}

void parseNamedUUIDs(const json& parent,
                    const char* key,
                    const std::string& context,
                    std::unordered_map<std::string, uci::base::UUID>& out) {
    requireArrayIfPresent(parent, key, context);
    auto it = parent.find(key);
    if (it == parent.end()) return;

    for (const auto& item : *it) {
        if (!item.is_object()) {
            throwUciException("CAL config: entries in " << context << "." << key << " must be objects");
        }
        const auto name = requireString(item, "name", context + "." + key + "[]");
        const auto uuid = requireUUID(item, "uuid", context + "." + key + "[" + name + "]");
        if (!out.emplace(name, uuid).second) {
            throwUciException("CAL config: duplicate " << key << " name '" << name << "' in selected system");
        }
    }
}

void parseServices(const json& parent,
                   const std::string& context,
                   const uci::base::UUID& subsystemUUID,
                   std::unordered_map<std::string, ServiceInfo>& out) {
    requireArrayIfPresent(parent, "services", context);
    auto it = parent.find("services");
    if (it == parent.end()) return;

    for (const auto& service : *it) {
        if (!service.is_object()) {
            throwUciException("CAL config: entries in " << context << ".services must be objects");
        }
        const auto name = requireString(service, "name", context + ".services[]");
        const auto uuid = requireUUID(service, "uuid", context + ".services[" + name + "]");
        if (!out.emplace(name, ServiceInfo{uuid, subsystemUUID}).second) {
            throwUciException("CAL config: duplicate service name '" << name << "' in selected system");
        }
    }
}

json readConfigFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throwUciException("CAL config: cannot open ARCAL_CONFIG path '" << path << "'");
    }

    try {
        json document;
        input >> document;
        return document;
    } catch (const std::exception& e) {
        throwUciException("CAL config: failed to parse '" << path << "': " << e.what());
    }
}

const json& selectSystem(const json& document) {
    if (!document.is_object()) {
        throwUciException("CAL config: root must be an object");
    }
    auto systemsIt = document.find("systems");
    if (systemsIt == document.end() || !systemsIt->is_array() || systemsIt->empty()) {
        throwUciException("CAL config: root must contain non-empty array 'systems'");
    }

    const char* selected = std::getenv("ARCAL_SYSTEM");
    const std::string selectedSystem = selected != nullptr ? selected : "";
    if (selectedSystem.empty()) {
        if (systemsIt->size() == 1) {
            if (!(*systemsIt)[0].is_object()) {
                throwUciException("CAL config: entries in systems must be objects");
            }
            return (*systemsIt)[0];
        }
        throwUciException("CAL config: ARCAL_SYSTEM must select one of multiple configured systems");
    }

    for (const auto& system : *systemsIt) {
        if (!system.is_object()) {
            throwUciException("CAL config: entries in systems must be objects");
        }
        if (requireString(system, "name", "systems[]") == selectedSystem) return system;
    }

    throwUciException("CAL config: ARCAL_SYSTEM '" << selectedSystem << "' is not present in config");
}

CalIdentity makeFallbackIdentity(const std::string& serviceLabel) {
    CalIdentity identity;
    identity.configured = false;
    identity.systemLabel = "system";
    identity.serviceLabel = serviceLabel;
    identity.systemUUID = uci::base::UUID::createVersion3UUID(uci::base::UUID::getNamespaceUUID(), "system");
    identity.serviceUUID = uci::base::UUID::createVersion3UUID(uci::base::UUID::getNamespaceUUID(), serviceLabel);
    identity.subsystemUUID = uci::base::UUID::createVersion3UUID(identity.serviceUUID, "subsystem");
    return identity;
}

} // namespace

CalIdentity resolveCalIdentity(const std::string& serviceLabel) {
    const char* configPathEnv = std::getenv("ARCAL_CONFIG");
    const std::string configPath = configPathEnv != nullptr ? configPathEnv : "";

    if (configPath == "NONE") {
        return makeFallbackIdentity(serviceLabel);
    }
    if (configPath.empty()) {
        throwUciException("CAL config: ARCAL_CONFIG must name a config file or be explicitly set to NONE");
    }

    const auto document = readConfigFile(configPath);
    const auto& system = selectSystem(document);
    const auto systemName = requireString(system, "name", "selected system");
    const auto systemUUID = requireUUID(system, "uuid", "system " + systemName);

    std::unordered_map<std::string, ServiceInfo> services;
    std::unordered_map<std::string, uci::base::UUID> components;
    std::unordered_map<std::string, uci::base::UUID> capabilities;

    parseServices(system, "system " + systemName, uci::base::UUID{}, services);
    parseNamedUUIDs(system, "components", "system " + systemName, components);
    parseNamedUUIDs(system, "capabilities", "system " + systemName, capabilities);

    requireArrayIfPresent(system, "subsystems", "system " + systemName);
    auto subsystemsIt = system.find("subsystems");
    if (subsystemsIt != system.end()) {
        std::unordered_set<std::string> subsystemNames;
        for (const auto& subsystem : *subsystemsIt) {
            if (!subsystem.is_object()) {
                throwUciException("CAL config: entries in system " << systemName << ".subsystems must be objects");
            }
            const auto subsystemName = requireString(subsystem, "name", "subsystems[]");
            if (!subsystemNames.insert(subsystemName).second) {
                throwUciException("CAL config: duplicate subsystem name '" << subsystemName << "' in selected system");
            }
            const auto subsystemUUID = requireUUID(subsystem, "uuid", "subsystem " + subsystemName);
            const auto context = "subsystem " + subsystemName;
            parseServices(subsystem, context, subsystemUUID, services);
            parseNamedUUIDs(subsystem, "components", context, components);
            parseNamedUUIDs(subsystem, "capabilities", context, capabilities);
        }
    }

    auto serviceIt = services.find(serviceLabel);
    if (serviceIt == services.end()) {
        throwUciException("CAL config: service '" << serviceLabel << "' is not configured in system '" << systemName << "'");
    }

    CalIdentity identity;
    identity.configured = true;
    identity.systemLabel = systemName;
    identity.serviceLabel = serviceLabel;
    identity.systemUUID = systemUUID;
    identity.serviceUUID = serviceIt->second.serviceUUID;
    identity.subsystemUUID = serviceIt->second.subsystemUUID;
    identity.componentUUIDs = std::move(components);
    identity.capabilityUUIDs = std::move(capabilities);
    return identity;
}

} // namespace config
} // namespace arcal
