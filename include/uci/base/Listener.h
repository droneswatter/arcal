#pragma once

namespace uci {
namespace base {

class Listener {
public:
    virtual ~Listener() = default;

protected:
    Listener() = default;
    Listener(const Listener&) = default;
    Listener& operator=(const Listener&) = default;
};

} // namespace base
} // namespace uci
