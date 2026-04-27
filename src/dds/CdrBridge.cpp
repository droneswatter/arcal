#include "arcal/CdrBridge.h"
#include "uci/base/UCIException.h"

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
    return obj.getAccessorType();
}

void cdrDeserialize(uint32_t tag, const std::vector<uint8_t>& in, uci::base::Accessor& obj) {
    if (!g_deserialize)
        throwUciException("CDR bridge not registered");
    g_deserialize(tag, in, 0, obj);
}

} // namespace arcal
