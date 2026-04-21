#pragma once

#include "BoundedList.h"

namespace uci {
namespace base {

// Concrete, publicly constructible BoundedList for use in generated type headers.
// BoundedList itself has a protected lifecycle to enforce spec conformance.
// MinOccurs/MaxOccurs are compile-time bounds forwarded to the base constructor.
template <typename T, uci::base::accessorType::AccessorType V,
          std::size_t MinOccurs = 0,
          std::size_t MaxOccurs = BoundedList<T, V>::UNBOUNDED_BOUND>
class BoundedListImpl : public BoundedList<T, V> {
public:
    BoundedListImpl() : BoundedList<T, V>(MinOccurs, MaxOccurs) {}
    BoundedListImpl(const BoundedListImpl&) = default;
    BoundedListImpl& operator=(const BoundedListImpl&) = default;
    ~BoundedListImpl() = default;
};

} // namespace base
} // namespace uci
