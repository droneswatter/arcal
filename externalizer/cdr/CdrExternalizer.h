#pragma once

#include "uci/base/Externalizer.h"

namespace arcal {
namespace externalizer {

static constexpr const char* CDR_ENCODING        = "CDR";
static constexpr const char* CAL_API_VERSION      = "2.5.0";
static constexpr const char* SCHEMA_VERSION       = "2.5.0";
static constexpr const char* VENDOR               = "arcal";
static constexpr const char* VENDOR_VERSION       = "0.1.0";

class CdrExternalizer : public uci::base::Externalizer {
public:
    CdrExternalizer() = default;
    ~CdrExternalizer() override = default;

    // Primary paths: vector<uint8_t> ↔ Accessor
    void read(const std::vector<uint8_t>& vec, uci::base::Accessor& type) override;
    void write(const uci::base::Accessor& type, std::vector<uint8_t>& vec) override;

    // Stream / string overloads delegate to the vector path
    void read(std::istream& istream, uci::base::Accessor& type) override;
    void read(const std::string& str, uci::base::Accessor& type) override;
    void write(const uci::base::Accessor& type, std::ostream& ostream) override;
    void write(const uci::base::Accessor& type, std::string& str) override;

    bool messageReadOnly()    const override { return false; }
    bool messageWriteOnly()   const override { return false; }
    bool supportsObjectRead()  const override { return true; }
    bool supportsObjectWrite() const override { return true; }

    std::string getCalApiVersion()  const override { return CAL_API_VERSION; }
    std::string getEncoding()       const override { return CDR_ENCODING; }
    std::string getSchemaVersion()  const override { return SCHEMA_VERSION; }
    std::string getVendorVersion()  const override { return VENDOR_VERSION; }
    std::string getVendor()         const override { return VENDOR; }
};

} // namespace externalizer
} // namespace arcal
