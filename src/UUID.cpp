#include "uci/base/UUID.h"
#include "uci/base/UCIException.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

// RFC 4122 UUID v4 (random) and v3 (name-based) implementation.
// All multi-byte fields stored in big-endian order in the octet array.

namespace {

void simpleNameHash(const uint8_t* ns, const uint8_t* name, std::size_t nameLen, uint8_t out[16]) {
    std::copy(ns, ns + 16, out);
    for (std::size_t i = 0; i < nameLen; ++i) {
        out[i % 16] ^= name[i];
        uint8_t carry = out[i % 16] >> 7;
        out[i % 16] = static_cast<uint8_t>((out[i % 16] << 1) | carry);
    }
    out[6] = (out[6] & 0x0F) | 0x30;
    out[8] = (out[8] & 0x3F) | 0x80;
}

} // namespace

namespace uci {
namespace base {

UUID::UUID(const ValueUUID& value) {
    for (int i = 0; i < 8; ++i)
        octets_[i]     = static_cast<uint8_t>((value.mostSignificantBits  >> (56 - 8 * i)) & 0xFF);
    for (int i = 0; i < 8; ++i)
        octets_[8 + i] = static_cast<uint8_t>((value.leastSignificantBits >> (56 - 8 * i)) & 0xFF);
}

UUID::UUID(const char* stringifiedUUID) {
    *this = fromString(std::string(stringifiedUUID));
}

UUID::UUID(uint64_t msb, uint64_t lsb) {
    for (int i = 0; i < 8; ++i)
        octets_[i]     = static_cast<uint8_t>((msb >> (56 - 8 * i)) & 0xFF);
    for (int i = 0; i < 8; ++i)
        octets_[8 + i] = static_cast<uint8_t>((lsb >> (56 - 8 * i)) & 0xFF);
}

UUID UUID::generateUUID() {
    UUID uuid;
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    for (auto& b : uuid.octets_) b = dist(gen);
    uuid.octets_[6] = (uuid.octets_[6] & 0x0F) | 0x40;
    uuid.octets_[8] = (uuid.octets_[8] & 0x3F) | 0x80;
    return uuid;
}

UUID UUID::createVersion3UUID(const UUID& ns, const std::string& name) {
    UUID uuid;
    simpleNameHash(ns.octets_.data(),
                   reinterpret_cast<const uint8_t*>(name.data()),
                   name.size(),
                   uuid.octets_.data());
    return uuid;
}

UUID UUID::createVersion3UUID(const std::string& name) {
    return createVersion3UUID(getNamespaceUUID(), name);
}

UUID UUID::fromString(const std::string& str) {
    if (str.size() != 36 || str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
        throwUciException("UUID::fromString: invalid format: " << str);

    UUID uuid;
    std::string hex;
    hex.reserve(32);
    for (char c : str) if (c != '-') hex += c;
    if (hex.size() != 32) throwUciException("UUID::fromString: invalid hex length");

    for (std::size_t i = 0; i < 16; ++i)
        uuid.octets_[i] = static_cast<uint8_t>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    return uuid;
}

UUID UUID::fromOctets(const Octets& octets) {
    UUID uuid;
    const std::size_t n = std::min(octets.size(), std::size_t{16});
    std::copy_n(octets.begin(), n, uuid.octets_.begin());
    return uuid;
}

UUID UUID::getNamespaceUUID() {
    return fromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
}

std::string UUID::toString() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) oss << '-';
        oss << std::setw(2) << static_cast<int>(octets_[i]);
    }
    return oss.str();
}

UUID::Octets UUID::getOctets() const {
    return Octets(octets_.begin(), octets_.end());
}

UUID::ValueUUID UUID::getValueUUID() const {
    ValueUUID v;
    for (int i = 0; i < 8; ++i) v.mostSignificantBits  = (v.mostSignificantBits  << 8) | octets_[i];
    for (int i = 8; i < 16; ++i) v.leastSignificantBits = (v.leastSignificantBits << 8) | octets_[i];
    return v;
}

UUID::Version UUID::getVersion() const {
    int raw = (octets_[6] >> 4) & 0x0F;
    switch (raw) {
        case 0: return versionNil;
        case 1: return versionTimeBased;
        case 2: return versionDceSecurity;
        case 3: return versionNameBasedMD5;
        case 4: return versionRandomNumber;
        case 5: return versionNameBasedSHA1;
        default: return versionUnknown;
    }
}

UUID::Variant UUID::getVariant() const {
    uint8_t v = octets_[8];
    if ((v & 0x80) == 0x00) return variantNCS;
    if ((v & 0xC0) == 0x80) return variantCurrent;
    if ((v & 0xE0) == 0xC0) return variantMicrosoft;
    if ((v & 0xE0) == 0xE0) return variantFuture;
    return variantUnknown;
}

UUID::Timestamp UUID::getTimestamp() const {
    uint64_t low  = (static_cast<uint64_t>(octets_[0]) << 24) |
                    (static_cast<uint64_t>(octets_[1]) << 16) |
                    (static_cast<uint64_t>(octets_[2]) <<  8) |
                     static_cast<uint64_t>(octets_[3]);
    uint64_t mid  = (static_cast<uint64_t>(octets_[4]) <<  8) |
                     static_cast<uint64_t>(octets_[5]);
    uint64_t high = (static_cast<uint64_t>(octets_[6] & 0x0F) << 8) |
                     static_cast<uint64_t>(octets_[7]);
    return (high << 48) | (mid << 32) | low;
}

UUID::ClockSequence UUID::getClockSequence() const {
    return static_cast<uint16_t>(((octets_[8] & 0x3F) << 8) | octets_[9]);
}

UUID::NodeAddress UUID::getNodeAddress() const {
    return NodeAddress(octets_.begin() + 10, octets_.end());
}

bool UUID::isNil() const {
    return std::all_of(octets_.begin(), octets_.end(), [](uint8_t b){ return b == 0; });
}

bool UUID::isValid() const { return !isNil(); }

const char* UUID::toString(Variant v) noexcept {
    switch (v) {
        case variantNCS:       return "NCS";
        case variantMicrosoft: return "Microsoft";
        case variantCurrent:   return "RFC4122";
        case variantFuture:    return "Future";
        default:               return "Unknown";
    }
}

const char* UUID::toString(Version v) noexcept {
    switch (v) {
        case versionNil:            return "Nil";
        case versionTimeBased:      return "TimeBased";
        case versionDceSecurity:    return "DCESecurity";
        case versionNameBasedMD5:   return "NameBasedMD5";
        case versionRandomNumber:   return "RandomNumber";
        case versionNameBasedSHA1:  return "NameBasedSHA1";
        default:                    return "Unknown";
    }
}

bool UUID::operator==(const UUID& rhs) const { return octets_ == rhs.octets_; }
bool UUID::operator!=(const UUID& rhs) const { return octets_ != rhs.octets_; }
bool UUID::operator<(const UUID& rhs)  const { return octets_ <  rhs.octets_; }
bool UUID::operator<=(const UUID& rhs) const { return octets_ <= rhs.octets_; }
bool UUID::operator>(const UUID& rhs)  const { return octets_ >  rhs.octets_; }
bool UUID::operator>=(const UUID& rhs) const { return octets_ >= rhs.octets_; }

} // namespace base
} // namespace uci

std::size_t std::hash<uci::base::UUID>::operator()(const uci::base::UUID& uuid) const noexcept {
    const auto octets = uuid.getOctets();
    std::size_t seed = 0;
    for (auto b : octets) seed ^= std::hash<uint8_t>{}(b) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}
