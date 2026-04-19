#include "JsonExternalizer.h"
#include "JsonRegistry.h"
#include "uci/base/Externalizer.h"
#include <mutex>

namespace arcal { namespace externalizer { namespace json {
    void register_all_json_handlers();
} } }

// ---------------------------------------------------------------------------
// Auto-registration: when arcal_externalizer_json.so is loaded via dlopen,
// register all generated JSON handlers so JsonExternalizer can dispatch by
// type name.
// ---------------------------------------------------------------------------

namespace {

struct JsonAutoRegister {
    JsonAutoRegister() {
        static std::once_flag flag;
        std::call_once(flag, [] {
            arcal::externalizer::json::register_all_json_handlers();
        });
    }
} g_jsonAutoRegister;

} // anonymous namespace

// ---------------------------------------------------------------------------
// Plugin entry point: resolved by dlsym in arcal's ExternalizerLoader.
// ---------------------------------------------------------------------------

extern "C" {

__attribute__((visibility("default")))
uci::base::Externalizer* arcal_create_externalizer() {
    return new arcal::externalizer::json::JsonExternalizer();
}

} // extern "C"
