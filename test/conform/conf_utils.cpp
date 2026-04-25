// Compile-only conformance for ARCAL's optional C++ utility helpers.
// These helpers are not part of CxxCAL; they wrap the public generated API.

#include "uci/type/ActionCommandMT.h"
#include "uci/utils/All.h"

#include <type_traits>

namespace {

void exercise_utils_surface(uci::base::AbstractServiceBusConnection* asb)
{
    auto connection = uci::utils::makeConnection("UtilityConformClient");
    auto message = uci::utils::makeMessage<uci::type::ActionCommandMT>(asb);
    auto reader = uci::utils::makeReader<uci::type::ActionCommandMT>("ActionCommand", asb);
    auto writer = uci::utils::makeWriter<uci::type::ActionCommandMT>("ActionCommand", asb);

    uci::utils::FunctionListener<uci::type::ActionCommandMT> listener(
        [](const uci::type::ActionCommandMT&) {});
    uci::utils::ScopedListener<uci::type::ActionCommandMT> scoped(reader.get(), listener);

    const auto uuid = uci::utils::newUuid();
    (void)connection;
    (void)message;
    (void)writer;
    (void)uuid;
}

static_assert(std::is_move_constructible_v<uci::utils::MessagePtr<uci::type::ActionCommandMT>>);
static_assert(!std::is_copy_constructible_v<uci::utils::MessagePtr<uci::type::ActionCommandMT>>);
static_assert(!std::is_copy_constructible_v<uci::utils::ReaderPtr<uci::type::ActionCommandMT>>);
static_assert(!std::is_copy_constructible_v<uci::utils::WriterPtr<uci::type::ActionCommandMT>>);
static_assert(!std::is_copy_constructible_v<uci::utils::ConnectionPtr>);

} // namespace
