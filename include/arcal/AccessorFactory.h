#pragma once

#include "uci/base/Accessor.h"
#include <cstdint>

namespace arcal {

// Returns a default-constructed Accessor of the message type identified by
// the 32-bit FNV-1a type tag, or nullptr if the tag is unknown.
// Only global element (message) types are registered — these are the only
// UCI types that appear as top-level tagged payloads on the DDS bus.
// Caller owns the returned object; release with arcalDestroyAccessor().
uci::base::Accessor* arcalCreateAccessor(uint32_t tag);
void arcalDestroyAccessor(uci::base::Accessor* acc);

} // namespace arcal
