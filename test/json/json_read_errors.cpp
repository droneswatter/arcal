// JSON externalizer — read error handling coverage.

#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/base/UCIException.h"
#include "uci/type/AccelerationChoiceType.h"
#include "uci/type/AtomicValueType.h"
#include "uci/type/HeaderType.h"
#include "uci/type/ObjectStateEnum.h"
#include "uci/type/UCI_SchemaVersionStringType.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

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
    auto* ext = loader->getExternalizer("JSON", "2.5.0", "2.5.0");

    uci::type::HeaderType header;
    uci::type::AccelerationChoiceType choice;
    uci::type::AtomicValueType atomic;
    uci::type::ObjectStateEnum state;
    uci::type::UCI_SchemaVersionStringType schemaVersion;

    expect_throw("malformed JSON", [&] {
        ext->read("{", header);
    });
    expect_throw("top-level non-object", [&] {
        ext->read("[]", choice);
    });
    expect_throw("missing required field", [&] {
        ext->read("{}", header);
    });
    expect_throw("wrong primitive type", [&] {
        ext->read("42", schemaVersion);
    });
    expect_throw("multiple choice variants", [&] {
        ext->read("{\"BooleanValue\":true,\"IntValue\":1}", atomic);
    });
    expect_throw("invalid enum string", [&] {
        ext->read("\"NOT_A_STATE\"", state);
    });

    loader->destroyExternalizer(ext);
    uci_destroyExternalizerLoader(loader);

    if (failures) {
        std::cerr << "FAIL json_read_errors — " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "PASS json_read_errors\n";
    return 0;
}
