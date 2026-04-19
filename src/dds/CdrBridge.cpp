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
        throwUciException("CDR externalizer not available — link against arcal_externalizer_cdr");
    return g_extFactory();
}

void cdrSerialize(const uci::base::Accessor& obj, std::vector<uint8_t>& out) {
    if (!g_serialize)
        throwUciException("CDR bridge not registered — link against arcal_externalizer_cdr");
    g_serialize(obj, out);
}

uint32_t cdrTypeTag(const uci::base::Accessor& obj) {
    return fnv1a32(obj.typeName().c_str());
}

void cdrDeserialize(uint32_t tag, const std::vector<uint8_t>& in, uci::base::Accessor& obj) {
    if (!g_deserialize)
        throwUciException("CDR bridge not registered — link against arcal_externalizer_cdr");
    g_deserialize(tag, in, 0, obj);
}

} // namespace arcal
