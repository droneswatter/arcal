#include "JsonExternalizer.h"
#include "JsonRegistry.h"
#include "uci/base/UCIException.h"

#include <ostream>

namespace arcal {
namespace externalizer {
namespace json {

void JsonExternalizer::write(const uci::base::Accessor& type, std::string& str) {
    try {
        JsonRegistry::instance().lookup(type.typeName()).serialize(type, str);
    } catch (const std::runtime_error& e) {
        throwUciException("JsonExternalizer::write: " << e.what());
    }
}

void JsonExternalizer::write(const uci::base::Accessor& type, std::ostream& ostream) {
    std::string str;
    write(type, str);
    ostream << str;
}

void JsonExternalizer::write(const uci::base::Accessor&, std::vector<uint8_t>&) {
    throwUciException("JsonExternalizer: binary write not supported; use write(type, string)");
}

void JsonExternalizer::read(std::istream&, uci::base::Accessor&) {
    throwUciException("JsonExternalizer: read not supported (write-only externalizer)");
}

void JsonExternalizer::read(const std::string&, uci::base::Accessor&) {
    throwUciException("JsonExternalizer: read not supported (write-only externalizer)");
}

void JsonExternalizer::read(const std::vector<uint8_t>&, uci::base::Accessor&) {
    throwUciException("JsonExternalizer: read not supported (write-only externalizer)");
}

} // namespace json
} // namespace externalizer
} // namespace arcal
