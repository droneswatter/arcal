#pragma once

#include "Accessor.h"
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace uci {
namespace base {

class Externalizer {
public:
    virtual void read(std::istream& istream, Accessor& type) = 0;
    virtual void read(const std::string& str, Accessor& type) = 0;
    virtual void read(const std::vector<uint8_t>& vec, Accessor& type) = 0;

    virtual void write(const Accessor& type, std::ostream& ostream) = 0;
    virtual void write(const Accessor& type, std::string& str) = 0;
    virtual void write(const Accessor& type, std::vector<uint8_t>& vec) = 0;

    virtual bool messageReadOnly() const = 0;
    virtual bool messageWriteOnly() const = 0;
    virtual bool supportsObjectRead() const = 0;
    virtual bool supportsObjectWrite() const = 0;

    virtual std::string getCalApiVersion() const = 0;
    virtual std::string getEncoding() const = 0;
    virtual std::string getSchemaVersion() const = 0;
    virtual std::string getVendorVersion() const = 0;
    virtual std::string getVendor() const = 0;

protected:
    friend class ExternalizerLoader;

    virtual ~Externalizer() = default;

    Externalizer() = default;
    Externalizer(const Externalizer&) = default;
    Externalizer& operator=(const Externalizer&) = default;
};

} // namespace base
} // namespace uci
