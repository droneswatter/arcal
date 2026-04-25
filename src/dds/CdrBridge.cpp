#include "arcal/CdrBridge.h"
#include "arcal/TypedAccessor.h"
#include "uci/base/UCIException.h"

#include <typeinfo>

namespace arcal {

static CdrSerializeFn        g_serialize      = nullptr;
static CdrDeserializeByTagFn g_deserialize    = nullptr;
static CdrExternalizerFactory g_extFactory    = nullptr;

void registerCdrBridge(CdrSerializeFn s, CdrDeserializeByTagFn d) {
    g_serialize   = s;
    g_deserialize = d;
}

void registerCdrExternalizerFactory(CdrExternalizerFactory f) {
    g_extFactory = f;
}

uci::base::Externalizer* arcalMakeCdrExternalizer() {
    if (!g_extFactory)
        throwUciException("CDR externalizer not available");
    return g_extFactory();
}

void cdrSerialize(const uci::base::Accessor& obj, std::vector<uint8_t>& out) {
    if (!g_serialize)
        throwUciException("CDR bridge not registered");
    g_serialize(obj, out);
}

uint32_t cdrTypeTag(const uci::base::Accessor& obj) {
    try {
        return dynamic_cast<const arcal::type::TypedAccessor&>(obj).typeTag();
    } catch (const std::bad_cast&) {
        throwUciException("CDR bridge cannot tag non-ARCAL typed accessor");
    }
}

void cdrDeserialize(uint32_t tag, const std::vector<uint8_t>& in, uci::base::Accessor& obj) {
    if (!g_deserialize)
        throwUciException("CDR bridge not registered");
    g_deserialize(tag, in, 0, obj);
}

} // namespace arcal
