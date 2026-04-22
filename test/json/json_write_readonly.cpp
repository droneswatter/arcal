// JSON externalizer — unsupported binary write path.
//
// Verifies that JSON advertises read/write object support and that
// write(vector) still throws (binary path unsupported for JSON).

#include "uci/type/ActionCommandMT.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/base/Externalizer.h"
#include "uci/base/UCIException.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <vector>

static int failures = 0;

static void expect_throw(const char* label, std::function<void()> fn) {
    try {
        fn();
        std::cerr << "FAIL: " << label << " — expected UCIException, got none\n";
        ++failures;
    } catch (const uci::base::UCIException&) {
        std::cout << "  ok: " << label << " threw UCIException\n";
    }
}

int main() {
    auto* loader = uci_getExternalizerLoader();
    auto* ext    = loader->getExternalizer("JSON", "2.5.0", "2.5.0");

    uci::type::ActionCommandMT msg;
    std::vector<uint8_t> vec;
    if (ext->messageReadOnly()) {
        std::cerr << "FAIL: messageReadOnly() should be false\n";
        ++failures;
    }
    if (ext->messageWriteOnly()) {
        std::cerr << "FAIL: messageWriteOnly() should be false\n";
        ++failures;
    }
    if (!ext->supportsObjectRead()) {
        std::cerr << "FAIL: supportsObjectRead() should be true\n";
        ++failures;
    }
    if (!ext->supportsObjectWrite()) {
        std::cerr << "FAIL: supportsObjectWrite() should be true\n";
        ++failures;
    }

    expect_throw("write(vector)",  [&]{ ext->write(msg, vec); });

    loader->destroyExternalizer(ext);
    uci_destroyExternalizerLoader(loader);

    if (failures) {
        std::cerr << "FAIL json_write_readonly — " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "PASS json_write_readonly\n";
    return 0;
}
