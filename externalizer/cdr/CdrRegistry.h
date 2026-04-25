#pragma once

// Runtime registry mapping accessor type names (and 32-bit FNV-1a tags) to
// CDR serialize/deserialize functions.
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
        table_[typeName] = handlers;
    }

    void registerByTag(uint32_t tag, const std::string& typeName, CdrHandlers handlers) {
        auto inserted = tagTable_.emplace(tag, handlers);
        if (!inserted.second)
            throw std::runtime_error("CdrRegistry: duplicate handler for type tag " + std::to_string(tag));
        tagNameTable_.emplace(tag, typeName);
    }

    const CdrHandlers& lookup(const std::string& typeName) const {
        auto it = table_.find(typeName);
        if (it == table_.end())
            throw std::runtime_error("CdrRegistry: no handler for type '" + typeName + "'");
        return it->second;
    }

    const CdrHandlers& lookupByTag(uint32_t tag) const {
        auto it = tagTable_.find(tag);
        if (it == tagTable_.end())
            throw std::runtime_error("CdrRegistry: no handler for type tag " + std::to_string(tag));
        return it->second;
    }

    // Returns empty string if tag is unknown.
    std::string typeNameForTag(uint32_t tag) const {
        auto it = tagNameTable_.find(tag);
        return it != tagNameTable_.end() ? it->second : std::string{};
    }

    bool has(const std::string& typeName) const {
        return table_.count(typeName) > 0;
    }

private:
    std::unordered_map<std::string,  CdrHandlers> table_;
    std::unordered_map<uint32_t,     CdrHandlers>  tagTable_;
    std::unordered_map<uint32_t,     std::string>  tagNameTable_;
};

} // namespace externalizer
} // namespace arcal
