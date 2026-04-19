#pragma once

#include "uci/base/Externalizer.h"

namespace arcal {
namespace externalizer {
namespace json {

class JsonExternalizer : public uci::base::Externalizer {
public:
    JsonExternalizer() = default;
    ~JsonExternalizer() override = default;

    void write(const uci::base::Accessor& type, std::string& str) override;
    void write(const uci::base::Accessor& type, std::ostream& ostream) override;
    void write(const uci::base::Accessor& type, std::vector<uint8_t>& vec) override;

    void read(std::istream& istream, uci::base::Accessor& type) override;
    void read(const std::string& str, uci::base::Accessor& type) override;
    void read(const std::vector<uint8_t>& vec, uci::base::Accessor& type) override;

    bool messageReadOnly()     const override { return false; }
    bool messageWriteOnly()    const override { return true; }
    bool supportsObjectRead()  const override { return false; }
    bool supportsObjectWrite() const override { return true; }

    std::string getCalApiVersion()  const override { return "2.5.0"; }
    std::string getEncoding()       const override { return "JSON"; }
    std::string getSchemaVersion()  const override { return "2.5.0"; }
    std::string getVendorVersion()  const override { return "0.1.0"; }
    std::string getVendor()         const override { return "arcal"; }
};

} // namespace json
} // namespace externalizer
} // namespace arcal
