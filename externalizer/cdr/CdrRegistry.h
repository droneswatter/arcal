#pragma once

// Runtime registry mapping accessor type names to CDR serialize/deserialize functions.
// Generated CDR handler files call CdrRegistry::registerHandler() at static init time.

#include "uci/base/Accessor.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace arcal {
namespace externalizer {

using CdrSerializeFn     = void(*)(const uci::base::Accessor&, std::vector<uint8_t>&);
using CdrDeserializeFn   = void(*)(const std::vector<uint8_t>&, uci::base::Accessor&);
using CdrDeserializeAtFn = void(*)(const std::vector<uint8_t>&, std::size_t&, uci::base::Accessor&);

struct CdrHandlers {
    CdrSerializeFn     serialize;
    CdrDeserializeFn   deserialize;
    CdrDeserializeAtFn deserialize_at;
};

class CdrRegistry {
public:
    static CdrRegistry& instance() {
        static CdrRegistry reg;
        return reg;
    }

    void registerHandler(const std::string& typeName, CdrHandlers handlers) {
        table_[typeName] = std::move(handlers);
    }

    const CdrHandlers& lookup(const std::string& typeName) const {
        auto it = table_.find(typeName);
        if (it == table_.end())
            throw std::runtime_error("CdrRegistry: no handler for type '" + typeName + "'");
        return it->second;
    }

    bool has(const std::string& typeName) const {
        return table_.count(typeName) > 0;
    }

private:
    std::unordered_map<std::string, CdrHandlers> table_;
};

// Utility: auto-register a handler at static init time
struct CdrHandlerRegistrar {
    CdrHandlerRegistrar(const std::string& typeName, CdrHandlers handlers) {
        CdrRegistry::instance().registerHandler(typeName, std::move(handlers));
    }
};

} // namespace externalizer
} // namespace arcal
