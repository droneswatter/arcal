#pragma once

namespace uci {
namespace base {

class Reader {
public:
    virtual ~Reader() = default;

protected:
    Reader() = default;
    Reader(const Reader&) = default;
    Reader& operator=(const Reader&) = default;
};

} // namespace base
} // namespace uci
