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
    const uint32_t tag = fnv1a32(obj.typeName().c_str());
    out.push_back(static_cast<uint8_t>(tag));
    out.push_back(static_cast<uint8_t>(tag >> 8));
    out.push_back(static_cast<uint8_t>(tag >> 16));
    out.push_back(static_cast<uint8_t>(tag >> 24));
    g_serialize(obj, out);
}

void cdrDeserialize(const std::vector<uint8_t>& in, uci::base::Accessor& obj) {
    if (!g_deserialize)
        throwUciException("CDR bridge not registered — link against arcal_externalizer_cdr");
    if (in.size() < 4)
        throwUciException("CDR bridge: payload too short to contain type tag");
    const uint32_t tag = static_cast<uint32_t>(in[0])
                       | static_cast<uint32_t>(in[1]) << 8
                       | static_cast<uint32_t>(in[2]) << 16
                       | static_cast<uint32_t>(in[3]) << 24;
    g_deserialize(tag, in, 4, obj);
}

} // namespace arcal
