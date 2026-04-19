#include "CdrExternalizer.h"
#include "CdrRegistry.h"
#include "arcal/CdrBridge.h"
#include "uci/base/UCIException.h"
#include <mutex>

namespace arcal { namespace externalizer { namespace cdr {
    void register_all_cdr_handlers();
} } }

// ---------------------------------------------------------------------------
// Auto-registration: when arcal_externalizer_cdr.so is loaded, wire CDR into
// both the transport bridge (DdsReader/DdsWriter) and the ExternalizerLoader
// factory slot (so uci_getExternalizerLoader can return a CdrExternalizer).
// ---------------------------------------------------------------------------

namespace {

void bridge_serialize(const uci::base::Accessor& obj, std::vector<uint8_t>& out) {
    arcal::externalizer::CdrRegistry::instance().lookup(obj.typeName()).serialize(obj, out);
}

void bridge_deserialize_by_tag(uint32_t tag, const std::vector<uint8_t>& in,
                                std::size_t offset, uci::base::Accessor& obj) {
    arcal::externalizer::CdrRegistry::instance().lookupByTag(tag).deserialize_at(in, offset, obj);
}

struct CdrAutoRegister {
    CdrAutoRegister() {
        static std::once_flag flag;
        std::call_once(flag, [] {
            arcal::externalizer::cdr::register_all_cdr_handlers();
            arcal::registerCdrBridge(bridge_serialize, bridge_deserialize_by_tag);
            arcal::registerCdrExternalizerFactory([] {
                return static_cast<uci::base::Externalizer*>(
                    new arcal::externalizer::CdrExternalizer());
            });
        });
    }
} g_cdrAutoRegister;

} // anonymous namespace

// ---------------------------------------------------------------------------
// Plugin entry point: called by ArcalExternalizerLoader via dlopen when the
// encoding is "CDR", and also available for direct use by tools that link
// against arcal_externalizer_cdr explicitly.
// ---------------------------------------------------------------------------

extern "C" {

__attribute__((visibility("default")))
uci::base::Externalizer* arcal_create_externalizer() {
    return new arcal::externalizer::CdrExternalizer();
}

} // extern "C"
