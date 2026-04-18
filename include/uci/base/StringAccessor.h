#pragma once

#include "Accessor.h"
#include <string>

namespace uci {
namespace base {

class StringAccessor : public Accessor {
public:
    AccessorType getAccessorType() const override { return ACCESSOR_TYPE_STRING; }
    void reset() override { value_.clear(); }

    const std::string& str() const { return value_; }
    const char* c_str() const { return value_.c_str(); }

    void setStringValue(const std::string& v) { value_ = v; }
    void setStringValue(const char* v) { value_ = v ? v : ""; }

    StringAccessor& operator=(const std::string& v) { value_ = v; return *this; }
    StringAccessor& operator=(const char* v) { setStringValue(v); return *this; }
    StringAccessor& operator=(const StringAccessor&) = default;

    operator std::string() const { return value_; }

    StringAccessor() = default;
    StringAccessor(const StringAccessor&) = default;
    ~StringAccessor() override = default;

private:
    std::string value_;
};

} // namespace base
} // namespace uci
