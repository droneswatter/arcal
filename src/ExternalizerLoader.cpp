#include "arcal/CdrBridge.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/base/UCIException.h"

#include <dlfcn.h>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Hardcoded encoding → plugin soname map.
// CDR is special-cased because it is built into libarcal.
// All other encodings are loaded on demand via dlopen.
// The fixed map prevents arbitrary .so injection via encoding strings.
// ---------------------------------------------------------------------------

static const std::unordered_map<std::string, const char*> PLUGIN_MAP = {
    {"JSON", "libarcal_externalizer_json.so"},
};

namespace arcal {

// Declared in CdrBridge.cpp — creates a built-in CdrExternalizer via the registered factory.
uci::base::Externalizer* arcalMakeCdrExternalizer();

namespace {

uci::base::Externalizer* loadPlugin(const char* soname) {
    void* handle = ::dlopen(soname, RTLD_NOW | RTLD_LOCAL);
    if (!handle)
        throwUciException("ExternalizerLoader: cannot load '" << soname << "': " << ::dlerror());

    using Factory = uci::base::Externalizer*(*)();
    auto factory = reinterpret_cast<Factory>(::dlsym(handle, "arcal_create_externalizer"));
    if (!factory)
        throwUciException("ExternalizerLoader: '" << soname
                          << "' does not export arcal_create_externalizer");
    return factory();
}

class ArcalExternalizerLoader : public uci::base::ExternalizerLoader {
public:
    uci::base::Externalizer* getExternalizer(const std::string& encoding,
                                              const std::string& /*schemaVersion*/,
                                              const std::string& /*calApiVersion*/) override {
        if (encoding == "CDR")
            return arcalMakeCdrExternalizer();

        auto it = PLUGIN_MAP.find(encoding);
        if (it == PLUGIN_MAP.end())
            throwUciException("ExternalizerLoader: unknown encoding '" << encoding << "'");

        return loadPlugin(it->second);
    }

    void destroyExternalizer(uci::base::Externalizer* ext) override {
        destroyExternalizerPointer(ext);
    }
};

} // anonymous namespace
} // namespace arcal

extern "C" {

uci::base::ExternalizerLoader* uci_getExternalizerLoader() {
    return new arcal::ArcalExternalizerLoader();
}

void uci_destroyExternalizerLoader(uci::base::ExternalizerLoader* loader) {
    delete loader;
}

} // extern "C"
