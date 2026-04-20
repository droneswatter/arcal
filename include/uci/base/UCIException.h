#pragma once

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

namespace uci {
namespace base {

class UCIException : public std::runtime_error {
public:
    using ErrorCode = uint32_t;

    explicit UCIException(const std::string& reason, ErrorCode errorCode = 0)
        : std::runtime_error(reason), errorCode_(errorCode) {}

    explicit UCIException(const char* reason, ErrorCode errorCode = 0)
        : std::runtime_error(reason), errorCode_(errorCode) {}

    explicit UCIException(const std::ostringstream& reason, ErrorCode errorCode = 0)
        : std::runtime_error(reason.str()), errorCode_(errorCode) {}

    virtual ErrorCode getErrorCode() const noexcept { return errorCode_; }

protected:
    ErrorCode errorCode_;
};

} // namespace base
} // namespace uci

#define throwUciException(msg) \
    do { \
        std::ostringstream _uci_oss; \
        _uci_oss << msg; \
        throw uci::base::UCIException(_uci_oss); \
    } while(0)
