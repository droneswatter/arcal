#pragma once
// Runtime registry mapping UCI type names to JSON serialize functions.

#include "uci/base/Accessor.h"
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace arcal {
namespace externalizer {
namespace json {

using JsonSerializeFn       = void(*)(const uci::base::Accessor&, std::string&);
using JsonSerializeFieldsFn = void(*)(const uci::base::Accessor&, std::string&, bool&);

struct JsonHandlers {
    JsonSerializeFn       serialize;        // emits { "k":v, ... }
    JsonSerializeFieldsFn serialize_fields; // emits  "k":v, ...  (no braces, for base expansion)
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
