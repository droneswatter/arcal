#include "uci/base/ExternalizerLoader.h"
#include "uci/type/ActionCommandMT.h"

#include <iostream>

int main() {
    auto& message = uci::type::ActionCommandMT::create(nullptr);
    (void)message;

    auto* loader = uci_getExternalizerLoader();
    if (loader == nullptr) {
        std::cerr << "uci_getExternalizerLoader returned null\n";
        return 1;
    }

    auto* externalizer = loader->getExternalizer("CDR", "2.5.0", "2.5.0");
    if (externalizer == nullptr) {
        std::cerr << "CDR externalizer lookup failed\n";
        uci_destroyExternalizerLoader(loader);
        return 2;
    }

    loader->destroyExternalizer(externalizer);
    uci_destroyExternalizerLoader(loader);
    uci::type::ActionCommandMT::destroy(message);

    std::cout << "PASS arcal install consumer smoke\n";
    return 0;
}
