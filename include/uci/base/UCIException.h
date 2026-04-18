#pragma once

#include <stdexcept>
#include <sstream>
#include <string>

namespace uci {
namespace base {

class UCIException : public std::runtime_error {
public:
    explicit UCIException(const std::string& reason)
        : std::runtime_error(reason), errorCode_(0) {}

    explicit UCIException(const char* reason)
        : std::runtime_error(reason), errorCode_(0) {}

    explicit UCIException(const std::ostringstream& reason)
        : std::runtime_error(reason.str()), errorCode_(0) {}

    virtual int getErrorCode() const { return errorCode_; }

protected:
    int errorCode_;
};

} // namespace base
} // namespace uci

#define throwUciException(msg) \
    do { \
        std::ostringstream _uci_oss; \
        _uci_oss << msg << " [" << __FILE__ << ":" << __LINE__ << "]"; \
        throw uci::base::UCIException(_uci_oss); \
    } while(0)
