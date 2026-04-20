#pragma once

namespace uci {
namespace base {

class Reader {
protected:
    Reader() = default;
    Reader(const Reader&) = default;
    Reader& operator=(const Reader&) = default;
    virtual ~Reader() = default;
};

} // namespace base
} // namespace uci
