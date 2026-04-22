#pragma once
// Runtime registry mapping UCI type names to JSON serialize/deserialize functions.

#include "uci/base/Accessor.h"
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace arcal {
namespace externalizer {
namespace json {

using JsonSerializeFn       = void(*)(const uci::base::Accessor&, std::string&);
using JsonSerializeFieldsFn = void(*)(const uci::base::Accessor&, std::string&, bool&);
using JsonDeserializeFn       = void(*)(const nlohmann::json&, uci::base::Accessor&);
using JsonDeserializeFieldsFn = void(*)(const nlohmann::json&, uci::base::Accessor&);

struct JsonHandlers {
    JsonSerializeFn       serialize;        // emits { "k":v, ... }
    JsonSerializeFieldsFn serialize_fields; // emits  "k":v, ...  (no braces, for base expansion)
    JsonDeserializeFn       deserialize;
    JsonDeserializeFieldsFn deserialize_fields;
};

class JsonRegistry {
public:
    static JsonRegistry& instance() {
        static JsonRegistry reg;
        return reg;
    }

    void registerHandler(const std::string& typeName, JsonHandlers h) {
        table_[typeName] = h;
    }

    const JsonHandlers& lookup(const std::string& typeName) const {
        auto it = table_.find(typeName);
        if (it == table_.end())
            throw std::runtime_error("JsonRegistry: no handler for type '" + typeName + "'");
        return it->second;
    }

    bool has(const std::string& typeName) const {
        return table_.count(typeName) > 0;
    }

private:
    std::unordered_map<std::string, JsonHandlers> table_;
};

} // namespace json
} // namespace externalizer
} // namespace arcal
