// Compile-only conformance: destructor visibility for API-owned lifetimes.
#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/Accessor.h"
#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/base/Listener.h"
#include "uci/base/Reader.h"
#include "uci/base/StringAccessor.h"
#include "uci/base/Writer.h"
#include "uci/type/ServiceStatusMT.h"

#include <type_traits>

static_assert(!std::is_destructible_v<uci::base::AbstractServiceBusConnection>,
    "AbstractServiceBusConnection destructor must be protected");
static_assert(!std::is_destructible_v<uci::base::Accessor>,
    "Accessor destructor must be protected");
static_assert(!std::is_destructible_v<uci::base::StringAccessor>,
    "StringAccessor destructor must be protected");
static_assert(!std::is_destructible_v<uci::base::Reader>,
    "Reader destructor must be protected");
static_assert(!std::is_destructible_v<uci::base::Writer>,
    "Writer destructor must be protected");
static_assert(!std::is_destructible_v<uci::base::Externalizer>,
    "Externalizer destructor must be protected");
static_assert(!std::is_destructible_v<uci::base::ExternalizerLoader>,
    "ExternalizerLoader destructor must be protected");

static_assert(!std::is_destructible_v<uci::type::ServiceStatusMT::Reader>,
    "Generated message Reader destructor must be protected");
static_assert(!std::is_destructible_v<uci::type::ServiceStatusMT::Writer>,
    "Generated message Writer destructor must be protected");

static_assert(std::has_virtual_destructor_v<uci::base::Listener>,
    "Listener destructor must be virtual");
static_assert(std::is_destructible_v<uci::base::Listener>,
    "Listener destructor must remain public");
static_assert(std::has_virtual_destructor_v<uci::base::AbstractServiceBusConnectionStatusListener>,
    "ASB status listener destructor must be virtual");
static_assert(std::is_destructible_v<uci::base::AbstractServiceBusConnectionStatusListener>,
    "ASB status listener destructor must remain public");
