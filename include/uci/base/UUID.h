#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace uci {
namespace base {

class UUID {
public:
    static constexpr std::size_t m_numberOfOctets   = 16;
    static constexpr std::size_t m_sizeOfNodeAddress = 6;

    using Octet         = uint8_t;
    using Octets        = std::vector<Octet>;
    using NodeAddress   = std::vector<uint8_t>;
    using Timestamp     = uint64_t;
    using ClockSequence = uint16_t;

    enum Variant {
        variantNCS, variantMicrosoft, variantCurrent, variantFuture, variantUnknown
    };
    enum Version {
        versionNil, versionTimeBased, versionDceSecurity,
        versionNameBasedMD5, versionRandomNumber, versionNameBasedSHA1, versionUnknown
    };

    struct ValueUUID {
        uint64_t mostSignificantBits{0};
        uint64_t leastSignificantBits{0};
    };

    static UUID fromString(const std::string& str);
    static UUID fromOctets(const Octets& octets);
    static UUID generateUUID();
    static UUID getNamespaceUUID();
    static UUID createVersion3UUID(const UUID& ns, const std::string& name);
    static UUID createVersion3UUID(const std::string& name);

    Octets      getOctets()       const;
    ValueUUID   getValueUUID()    const;
    std::string toString()        const;
    Variant     getVariant()      const;
    Version     getVersion()      const;
    ClockSequence getClockSequence() const;
    Timestamp   getTimestamp()    const;
    NodeAddress getNodeAddress()  const;
    bool isNil()   const;
    bool isValid() const;

    static const char* toString(Variant v) noexcept;
    static const char* toString(Version v) noexcept;

    UUID();
    explicit UUID(const ValueUUID& value);
    UUID(const char* stringifiedUUID);
    UUID(uint64_t mostSignificantBits, uint64_t leastSignificantBits);
    UUID(const UUID&) = default;
    UUID& operator=(const UUID&) = default;

    bool operator==(const UUID& rhs) const;
    bool operator!=(const UUID& rhs) const;
    bool operator<(const UUID& rhs)  const;
    bool operator<=(const UUID& rhs) const;
    bool operator>(const UUID& rhs)  const;
    bool operator>=(const UUID& rhs) const;

private:
    using OctetArray = std::array<uint8_t, 16>;
    OctetArray octets_{};
};

} // namespace base
} // namespace uci

template <>
struct std::hash<uci::base::UUID> {
    std::size_t operator()(const uci::base::UUID& uuid) const noexcept;
};
