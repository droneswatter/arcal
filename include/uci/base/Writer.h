#pragma once

namespace uci {
namespace base {

class Writer {
public:
    virtual ~Writer() = default;

protected:
    Writer() = default;
    Writer(const Writer&) = default;
    Writer& operator=(const Writer&) = default;
};

} // namespace base
} // namespace uci
