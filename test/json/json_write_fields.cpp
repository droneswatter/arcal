// JSON externalizer — field serialization correctness.
//
// Verifies that write(accessor, string) produces well-formed JSON containing
// the expected keys and values for a populated ActionCommandMT message.

#include "uci/type/ActionCommandMT.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/base/Externalizer.h"
#include "uci/base/UCIException.h"
#include <cassert>
#include <iostream>
#include <string>

static void require(bool cond, const char* what) {
    if (!cond) {
        std::cerr << "FAIL: " << what << "\n";
        std::exit(1);
    }
}

int main() {
    // Obtain JSON externalizer via the standard CAL loader API.
    auto* loader = uci_getExternalizerLoader();
    require(loader != nullptr, "uci_getExternalizerLoader returned null");

    auto* ext = loader->getExternalizer("JSON", "2.5.0", "2.5.0");
    require(ext != nullptr, "getExternalizer(\"JSON\") returned null");

    // Metadata checks.
    require(ext->getEncoding()      == "JSON",  "getEncoding() == JSON");
    require(ext->getVendor()        == "arcal", "getVendor() == arcal");
    require(ext->messageWriteOnly() == false,   "messageWriteOnly() == false");
    require(ext->supportsObjectRead()  == true, "supportsObjectRead() == true");
    require(ext->supportsObjectWrite() == true,  "supportsObjectWrite() == true");

    // Populate a message with a known sentinel value.
    static const std::string kSentinel{"arcal-json-sentinel"};
    auto& msg = uci::type::ActionCommandMT::create(nullptr);
    msg.getMessageHeader().getSchemaVersion().setValue(kSentinel);

    // Serialize to JSON string.
    std::string json;
    ext->write(msg, json);

    require(!json.empty(),         "JSON output is non-empty");
    require(json.front() == '{',   "JSON starts with '{'");
    require(json.back()  == '}',   "JSON ends with '}'");

    // Structural checks — no JSON parser needed, just substring presence.
    require(json.find("\"MessageHeader\"") != std::string::npos,
            "JSON contains MessageHeader key");
    require(json.find("\"SchemaVersion\"") != std::string::npos,
            "JSON contains SchemaVersion key");
    require(json.find(kSentinel) != std::string::npos,
            "JSON contains sentinel value");

    loader->destroyExternalizer(ext);
    uci_destroyExternalizerLoader(loader);
    uci::type::ActionCommandMT::destroy(msg);

    std::cout << "PASS json_write_fields\n";
    std::cout << "JSON: " << json << "\n";
    return 0;
}
