#pragma once

#include "SimpleList.h"

namespace uci {
namespace base {

// Concrete, publicly constructible SimpleList for use in generated type headers.
// SimpleList itself has a protected lifecycle to enforce spec conformance.
template <typename T, uci::base::accessorType::AccessorType V>
class SimpleListImpl : public SimpleList<T, V> {
public:
    SimpleListImpl() = default;
    SimpleListImpl(const SimpleListImpl&) = default;
    SimpleListImpl& operator=(const SimpleListImpl&) = default;
    ~SimpleListImpl() = default;
};

} // namespace base
} // namespace uci
