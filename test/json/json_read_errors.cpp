// JSON externalizer — read error handling coverage.

#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/base/UCIException.h"
#include "uci/type/HeaderType.h"
#include "uci/type/NameValuePairValueType.h"
#include "uci/type/SystemStateEnum.h"

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

    auto& header = uci::type::HeaderType::create(nullptr);
    auto& choice = uci::type::NameValuePairValueType::create(nullptr);
    auto& state = uci::type::SystemStateEnum::create(nullptr);

    header.getSystemID().setUUID(uci::base::UUID::createVersion3UUID("json-read-errors-system"));
    header.getTimestamp().setValue(1777248000);
    header.getSchemaVersion() = "2.5.0";
    header.setMode(uci::type::MessageModeEnum::LIVE);

    std::string validHeaderJson;
    ext->write(header, validHeaderJson);
    std::string wrongSchemaTypeJson = validHeaderJson;
    const std::string quotedSchemaVersion = "\"SchemaVersion\":\"2.5.0\"";
    const auto schemaPos = wrongSchemaTypeJson.find(quotedSchemaVersion);
    if (schemaPos == std::string::npos) {
        std::cerr << "FAIL: could not locate SchemaVersion in serialized header\n";
        return 1;
    }
    wrongSchemaTypeJson.replace(schemaPos, quotedSchemaVersion.size(), "\"SchemaVersion\":42");

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
        ext->read(wrongSchemaTypeJson, header);
    });
    expect_throw("multiple choice variants", [&] {
        ext->read("{\"BooleanValue\":true,\"IntValue\":1}", choice);
    });
    expect_throw("invalid enum string", [&] {
        ext->read("\"NOT_A_STATE\"", state);
    });

    loader->destroyExternalizer(ext);
    uci_destroyExternalizerLoader(loader);
    uci::type::SystemStateEnum::destroy(state);
    uci::type::NameValuePairValueType::destroy(choice);
    uci::type::HeaderType::destroy(header);

    if (failures) {
        std::cerr << "FAIL json_read_errors — " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "PASS json_read_errors\n";
    return 0;
}
