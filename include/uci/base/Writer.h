#pragma once

namespace uci {
namespace base {

class Writer {
protected:
    Writer() = default;
    Writer(const Writer&) = default;
    Writer& operator=(const Writer&) = default;
    virtual ~Writer() = default;
};

} // namespace base
} // namespace uci
