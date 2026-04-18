#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

namespace uci {
namespace base {

class UUID {
public:
    using OctetArray = std::array<uint8_t, 16>;

    static UUID generateUUID();
    static UUID createVersion3UUID(const UUID& ns, const std::string& name);
    static UUID fromString(const std::string& str);
    static UUID fromOctets(const OctetArray& octets);
    static const UUID& getNamespaceUUID();

    std::string toString() const;
    OctetArray getOctets() const;
    uint64_t getValueUUID() const;

    int getVariant() const;
    int getVersion() const;
    uint64_t getTimestamp() const;
    uint16_t getClockSequence() const;
    uint64_t getNodeAddress() const;

    bool isNil() const;
    bool isValid() const;

    bool operator==(const UUID& rhs) const;
    bool operator!=(const UUID& rhs) const;
    bool operator<(const UUID& rhs) const;
    bool operator<=(const UUID& rhs) const;
    bool operator>(const UUID& rhs) const;
    bool operator>=(const UUID& rhs) const;

    UUID();
    UUID(const UUID&) = default;
    UUID& operator=(const UUID&) = default;

private:
    OctetArray octets_{};
};

} // namespace base
} // namespace uci

template <>
struct std::hash<uci::base::UUID> {
    std::size_t operator()(const uci::base::UUID& uuid) const noexcept;
};
