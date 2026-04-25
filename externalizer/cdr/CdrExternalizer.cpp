#include "CdrExternalizer.h"
#include "CdrRegistry.h"
#include "CdrPrimitives.h"
#include "arcal/TypedAccessor.h"
#include "uci/base/UCIException.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <typeinfo>

namespace arcal {
namespace externalizer {

// ---------------------------------------------------------------------------
// Stream / string ↔ vector adapters
// ---------------------------------------------------------------------------

void CdrExternalizer::read(std::istream& istream, uci::base::Accessor& type) {
    std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(istream), {});
    read(bytes, type);
}

void CdrExternalizer::read(const std::string& str, uci::base::Accessor& type) {
    std::vector<uint8_t> bytes(str.begin(), str.end());
    read(bytes, type);
}

void CdrExternalizer::write(const uci::base::Accessor& type, std::ostream& ostream) {
    std::vector<uint8_t> bytes;
    write(type, bytes);
    ostream.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
}

void CdrExternalizer::write(const uci::base::Accessor& type, std::string& str) {
    std::vector<uint8_t> bytes;
    write(type, bytes);
    str.assign(bytes.begin(), bytes.end());
}

// ---------------------------------------------------------------------------
// Primary dispatch through the CDR type registry
// ---------------------------------------------------------------------------

void CdrExternalizer::read(const std::vector<uint8_t>& vec, uci::base::Accessor& type) {
    try {
        const auto& typed = dynamic_cast<const arcal::type::TypedAccessor&>(type);
        CdrRegistry::instance().lookupByTag(typed.typeTag()).deserialize(vec, type);
    } catch (const std::runtime_error& e) {
        throwUciException("CdrExternalizer::read: " << e.what());
    } catch (const std::bad_cast&) {
        throwUciException("CdrExternalizer::read: accessor is not an ARCAL typed accessor");
    }
}

void CdrExternalizer::write(const uci::base::Accessor& type, std::vector<uint8_t>& vec) {
    try {
        const auto& typed = dynamic_cast<const arcal::type::TypedAccessor&>(type);
        CdrRegistry::instance().lookupByTag(typed.typeTag()).serialize(type, vec);
    } catch (const std::runtime_error& e) {
        throwUciException("CdrExternalizer::write: " << e.what());
    } catch (const std::bad_cast&) {
        throwUciException("CdrExternalizer::write: accessor is not an ARCAL typed accessor");
    }
}

} // namespace externalizer
} // namespace arcal
