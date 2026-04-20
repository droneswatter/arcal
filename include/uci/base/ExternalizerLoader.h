#pragma once

#include "Externalizer.h"
#include <string>

#if defined(__GNUC__) || defined(__clang__)
#  define ARCAL_API __attribute__((visibility("default")))
#else
#  define ARCAL_API
#endif

namespace uci { namespace base { class ExternalizerLoader; } }

extern "C" {
    ARCAL_API uci::base::ExternalizerLoader* uci_getExternalizerLoader();
    ARCAL_API void uci_destroyExternalizerLoader(uci::base::ExternalizerLoader* loader);
}

namespace uci {
namespace base {

class ExternalizerLoader {
public:
    virtual Externalizer* getExternalizer(const std::string& encoding,
                                          const std::string& schemaVersion,
                                          const std::string& calApiVersion) = 0;

    virtual void destroyExternalizer(Externalizer* externalizer) = 0;

protected:
    friend void ::uci_destroyExternalizerLoader(uci::base::ExternalizerLoader* loader);

    static void destroyExternalizerPointer(Externalizer* externalizer) {
        delete externalizer;
    }

    virtual ~ExternalizerLoader() = default;

    ExternalizerLoader() = default;
    ExternalizerLoader(const ExternalizerLoader&) = default;
    ExternalizerLoader& operator=(const ExternalizerLoader&) = default;
};

} // namespace base
} // namespace uci
