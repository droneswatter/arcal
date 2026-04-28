// Compile-only conformance for ARCAL's optional C++ utility helpers.
// These helpers are not part of CxxCAL; they wrap the public generated API.

#include "uci/type/ServiceStatusMT.h"
#include "uci/utils/All.h"

#include <type_traits>

namespace {

void exercise_utils_surface(uci::base::AbstractServiceBusConnection* asb)
{
    auto connection = uci::utils::makeConnection("UtilityConformClient");
    auto message = uci::utils::makeMessage<uci::type::ServiceStatusMT>(asb);
    auto reader = uci::utils::makeReader<uci::type::ServiceStatusMT>("ServiceStatus", asb);
    auto writer = uci::utils::makeWriter<uci::type::ServiceStatusMT>("ServiceStatus", asb);

    uci::utils::MessageListener<uci::type::ServiceStatusMT> listener(
        [](const uci::type::ServiceStatusMT&) {});
    uci::utils::ScopedListener<uci::type::ServiceStatusMT> scoped(reader.get(), listener);

    (void)connection;
    (void)message;
    (void)writer;
}

static_assert(std::is_move_constructible_v<uci::utils::MessagePtr<uci::type::ServiceStatusMT>>);
static_assert(!std::is_copy_constructible_v<uci::utils::MessagePtr<uci::type::ServiceStatusMT>>);
static_assert(!std::is_copy_constructible_v<uci::utils::ReaderPtr<uci::type::ServiceStatusMT>>);
static_assert(!std::is_copy_constructible_v<uci::utils::WriterPtr<uci::type::ServiceStatusMT>>);
static_assert(!std::is_copy_constructible_v<uci::utils::ConnectionPtr>);

} // namespace
