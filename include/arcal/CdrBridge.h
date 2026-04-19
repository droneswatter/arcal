#pragma once

#include "uci/base/Accessor.h"
#include "uci/base/Externalizer.h"
#include <cstdint>
#include <vector>

namespace arcal {

// FNV-1a 32-bit — constexpr so tags can be used in switch statements or
// computed at compile time in generated code.
constexpr uint32_t fnv1a32(const char* s, uint32_t h = 2166136261u) {
    return *s ? fnv1a32(s + 1, (h ^ static_cast<uint8_t>(*s)) * 16777619u) : h;
}

// Internal CDR dispatch — wired by arcal_externalizer_cdr at shared-library
// load time via a static initializer.  DdsReader and DdsWriter call these
// instead of going through the Externalizer API, keeping CDR an internal
// transport detail invisible to application code.
//
// Wire format: [ type_tag: uint32_t LE | CDR payload bytes... ]
// The tag is the FNV-1a 32-bit hash of the UCI type name.
using CdrSerializeFn        = void(*)(const uci::base::Accessor&, std::vector<uint8_t>&);
using CdrDeserializeByTagFn = void(*)(uint32_t tag, const std::vector<uint8_t>&,
                                      std::size_t offset, uci::base::Accessor&);

void registerCdrBridge(CdrSerializeFn s, CdrDeserializeByTagFn d);
void     cdrSerialize  (const uci::base::Accessor& obj, std::vector<uint8_t>& out);
uint32_t cdrTypeTag    (const uci::base::Accessor& obj);
void     cdrDeserialize(uint32_t tag, const std::vector<uint8_t>& in, uci::base::Accessor& obj);

// Called by arcal_externalizer_cdr at load time so the ExternalizerLoader in
// arcal core can instantiate CdrExternalizer without a circular link dependency.
using CdrExternalizerFactory = uci::base::Externalizer*(*)();
void registerCdrExternalizerFactory(CdrExternalizerFactory f);

} // namespace arcal
