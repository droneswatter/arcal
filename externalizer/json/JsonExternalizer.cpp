#include "JsonExternalizer.h"
#include "JsonParse.h"
#include "JsonRegistry.h"
#include "uci/base/UCIException.h"

#include <nlohmann/json.hpp>
#include <ostream>
#include <sstream>

namespace arcal {
namespace externalizer {
namespace json {

void JsonExternalizer::write(const uci::base::Accessor& type, std::string& str) {
    try {
        JsonRegistry::instance().lookup(type.typeName()).serialize(type, str);
    } catch (const uci::base::UCIException&) {
        throw;
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

void JsonExternalizer::read(std::istream& istream, uci::base::Accessor& type) {
    std::ostringstream out;
    out << istream.rdbuf();
    read(out.str(), type);
}

void JsonExternalizer::read(const std::string& str, uci::base::Accessor& type) {
    try {
        auto doc = nlohmann::json::parse(str);
        type.reset();
        JsonRegistry::instance().lookup(type.typeName()).deserialize(doc, type);
    } catch (const uci::base::UCIException&) {
        throw;
    } catch (const nlohmann::json::exception& e) {
        throwUciException("JsonExternalizer::read: " << e.what());
    } catch (const std::runtime_error& e) {
        throwUciException("JsonExternalizer::read: " << e.what());
    }
}

void JsonExternalizer::read(const std::vector<uint8_t>& vec, uci::base::Accessor& type) {
    read(std::string(vec.begin(), vec.end()), type);
}

} // namespace json
} // namespace externalizer
} // namespace arcal
