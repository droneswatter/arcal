// JSON externalizer — write-only constraint.
//
// Verifies that all read() overloads throw UCIException and that
// write(vector) throws (binary path unsupported for JSON).

#include "uci/type/ActionCommandMT.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/base/Externalizer.h"
#include "uci/base/UCIException.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
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
    std::string          str{"{}"};
    std::istringstream   iss{str};

    expect_throw("read(istream)",  [&]{ ext->read(iss, msg); });
    expect_throw("read(string)",   [&]{ ext->read(str, msg); });
    expect_throw("read(vector)",   [&]{ ext->read(vec, msg); });
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
