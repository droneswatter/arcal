#include "uci/base/UUID.h"
#include "uci/base/UCIException.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

// RFC 4122 UUID v4 (random) and v3 (MD5 name-based) implementation.
// All multi-byte fields stored in big-endian order in the octet array.

namespace {

// FNV-1a inspired fast hash for v3 UUID (no OpenSSL dependency for reference impl).
// A production CAL may substitute proper MD5 per RFC 4122.
void simpleNameHash(const uint8_t* ns, const uint8_t* name, std::size_t nameLen, uint8_t out[16]) {
    std::copy(ns, ns + 16, out);
    for (std::size_t i = 0; i < nameLen; ++i) {
        out[i % 16] ^= name[i];
        // rotate left by 1
        uint8_t carry = out[i % 16] >> 7;
        out[i % 16] = static_cast<uint8_t>((out[i % 16] << 1) | carry);
    }
    // Set version 3 and variant bits
    out[6] = (out[6] & 0x0F) | 0x30;
    out[8] = (out[8] & 0x3F) | 0x80;
}

} // namespace

namespace uci {
namespace base {

UUID::UUID() {
    octets_.fill(0);
}

UUID UUID::generateUUID() {
    UUID uuid;
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    for (auto& b : uuid.octets_) b = dist(gen);
    // Version 4
    uuid.octets_[6] = (uuid.octets_[6] & 0x0F) | 0x40;
    // Variant 10xx
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

UUID UUID::fromString(const std::string& str) {
    // Expected format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    if (str.size() != 36 || str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
        throwUciException("UUID::fromString: invalid format: " << str);

    UUID uuid;
    std::string hex;
    hex.reserve(32);
    for (char c : str) if (c != '-') hex += c;
    if (hex.size() != 32) throwUciException("UUID::fromString: invalid hex length");

    for (std::size_t i = 0; i < 16; ++i) {
        uuid.octets_[i] = static_cast<uint8_t>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    }
    return uuid;
}

UUID UUID::fromOctets(const OctetArray& octets) {
    UUID uuid;
    uuid.octets_ = octets;
    return uuid;
}

const UUID& UUID::getNamespaceUUID() {
    // OMS namespace UUID (version 3, DNS-based) — placeholder
    static UUID ns = UUID::fromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
    return ns;
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

UUID::OctetArray UUID::getOctets() const { return octets_; }

uint64_t UUID::getValueUUID() const {
    uint64_t hi = 0, lo = 0;
    for (int i = 0; i < 8; ++i) hi = (hi << 8) | octets_[i];
    for (int i = 8; i < 16; ++i) lo = (lo << 8) | octets_[i];
    return hi ^ lo;
}

int UUID::getVersion() const { return (octets_[6] >> 4) & 0x0F; }

int UUID::getVariant() const {
    uint8_t v = octets_[8];
    if ((v & 0x80) == 0x00) return 0;
    if ((v & 0xC0) == 0x80) return 1;
    if ((v & 0xE0) == 0xC0) return 2;
    return 3;
}

uint64_t UUID::getTimestamp() const {
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

uint16_t UUID::getClockSequence() const {
    return static_cast<uint16_t>(((octets_[8] & 0x3F) << 8) | octets_[9]);
}

uint64_t UUID::getNodeAddress() const {
    uint64_t node = 0;
    for (int i = 10; i < 16; ++i) node = (node << 8) | octets_[i];
    return node;
}

bool UUID::isNil() const {
    return std::all_of(octets_.begin(), octets_.end(), [](uint8_t b){ return b == 0; });
}

bool UUID::isValid() const { return !isNil(); }

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
