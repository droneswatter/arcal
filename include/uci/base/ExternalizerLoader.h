#pragma once

#include "Externalizer.h"
#include <string>

namespace uci {
namespace base {

class ExternalizerLoader {
public:
    virtual Externalizer* getExternalizer(const std::string& encoding,
                                          const std::string& schemaVersion,
                                          const std::string& calApiVersion) = 0;

    virtual void destroyExternalizer(Externalizer* externalizer) = 0;

    virtual ~ExternalizerLoader() = default;

protected:
    ExternalizerLoader() = default;
    ExternalizerLoader(const ExternalizerLoader&) = default;
    ExternalizerLoader& operator=(const ExternalizerLoader&) = default;
};

} // namespace base
} // namespace uci

#if defined(__GNUC__) || defined(__clang__)
#  define ARCAL_API __attribute__((visibility("default")))
#else
#  define ARCAL_API
#endif

extern "C" {
    ARCAL_API uci::base::ExternalizerLoader* uci_getExternalizerLoader();
    ARCAL_API void uci_destroyExternalizerLoader(uci::base::ExternalizerLoader* loader);
}
