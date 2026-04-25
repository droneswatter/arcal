#pragma once

namespace uci {
namespace utils {

template <typename EnumAccessor>
void set(EnumAccessor& accessor, typename EnumAccessor::EnumerationItem value) {
    accessor.setValue(value);
}

} // namespace utils
} // namespace uci
