#pragma once

#include "Accessor.h"
#include <string>

namespace uci {
namespace base {

class StringAccessor : public Accessor {
public:
    AccessorType getAccessorType() const noexcept override { return ACCESSOR_TYPE_STRING; }
    void reset() override { value_.clear(); }

    std::string str() const { return value_; }
    const char* c_str() const { return value_.c_str(); }

    StringAccessor& setStringValue(const std::string& v) { value_ = v; return *this; }
    StringAccessor& setStringValue(const char* v) { value_ = v ? v : ""; return *this; }

    StringAccessor& operator=(const std::string& v) { value_ = v; return *this; }
    StringAccessor& operator=(const char* v) { setStringValue(v); return *this; }
    StringAccessor& operator=(const StringAccessor&) = default;

    operator std::string() const { return value_; }

private:
    std::string value_;

protected:
    StringAccessor() = default;
    StringAccessor(const StringAccessor&) = default;
    ~StringAccessor() override = default;
};

} // namespace base
} // namespace uci
