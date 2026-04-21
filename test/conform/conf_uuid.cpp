// Compile-only conformance: UUID API shape (OMSC-SPC-008 RevK).

#include "uci/base/UUID.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

using UUID = uci::base::UUID;

static_assert(UUID::m_numberOfOctets == 16,
    "UUID must define public static const size_t m_numberOfOctets == 16");
static_assert(UUID::m_sizeOfNodeAddress == 6,
    "UUID must define public static const size_t m_sizeOfNodeAddress == 6");

static_assert(std::is_same_v<UUID::Octet, uint8_t>);
static_assert(std::is_same_v<UUID::Octets, std::vector<UUID::Octet>>);
static_assert(std::is_same_v<UUID::NodeAddress, std::vector<uint8_t>>);
static_assert(std::is_same_v<UUID::Timestamp, uint64_t>);
static_assert(std::is_same_v<UUID::ClockSequence, uint16_t>);

static_assert(std::is_enum_v<UUID::Variant>);
static_assert(std::is_enum_v<UUID::Version>);
static_assert(std::is_class_v<UUID::ValueUUID>);
static_assert(std::is_same_v<decltype(std::declval<UUID::ValueUUID>().mostSignificantBits), uint64_t>);
static_assert(std::is_same_v<decltype(std::declval<UUID::ValueUUID>().leastSignificantBits), uint64_t>);

static_assert(std::is_same_v<decltype(UUID::fromString(std::declval<const std::string&>())), UUID>);
static_assert(std::is_same_v<decltype(UUID::fromOctets(std::declval<const UUID::Octets&>())), UUID>);
static_assert(std::is_same_v<decltype(UUID::generateUUID()), UUID>);
static_assert(std::is_same_v<decltype(UUID::getNamespaceUUID()), UUID>);
static_assert(std::is_same_v<decltype(UUID::createVersion3UUID(
    std::declval<const UUID&>(), std::declval<const std::string&>())), UUID>);
static_assert(std::is_same_v<decltype(UUID::createVersion3UUID(std::declval<const std::string&>())), UUID>);

static_assert(std::is_same_v<decltype(std::declval<const UUID&>().getOctets()), UUID::Octets>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>().getValueUUID()), UUID::ValueUUID>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>().toString()), std::string>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>().getVariant()), UUID::Variant>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>().getVersion()), UUID::Version>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>().getClockSequence()), UUID::ClockSequence>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>().getTimestamp()), UUID::Timestamp>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>().getNodeAddress()), UUID::NodeAddress>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>().isNil()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>().isValid()), bool>);

static_assert(noexcept(UUID::toString(std::declval<UUID::Variant>())));
static_assert(noexcept(UUID::toString(std::declval<UUID::Version>())));
static_assert(std::is_same_v<decltype(UUID::toString(std::declval<UUID::Variant>())), const char*>);
static_assert(std::is_same_v<decltype(UUID::toString(std::declval<UUID::Version>())), const char*>);

static_assert(std::is_constructible_v<UUID, const UUID::ValueUUID&>);
static_assert(std::is_constructible_v<UUID, const char*>);
static_assert(std::is_constructible_v<UUID, uint64_t, uint64_t>);
static_assert(std::is_copy_constructible_v<UUID>);
static_assert(std::is_assignable_v<UUID&, const UUID&>);
static_assert(std::is_default_constructible_v<UUID>);

static_assert(std::is_same_v<decltype(std::declval<const UUID&>() == std::declval<const UUID&>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>() != std::declval<const UUID&>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>() < std::declval<const UUID&>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>() <= std::declval<const UUID&>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>() > std::declval<const UUID&>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const UUID&>() >= std::declval<const UUID&>()), bool>);
