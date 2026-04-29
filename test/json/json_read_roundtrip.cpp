// JSON externalizer — read/write round-trip coverage.

#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/type/NameValuePairValueType.h"
#include "uci/type/ServiceStatusMT.h"
#include "uci/type/ServiceStateEnum.h"
#include "uci/type/ProcessingStatusEnum.h"
#include "uci/type/SubsystemStatusMDT.h"
#include "uci/type/SubsystemStateEnum.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void require(bool cond, const char* what) {
    if (!cond) {
        std::cerr << "FAIL: " << what << "\n";
        std::exit(1);
    }
}

static void roundtrip(uci::base::Externalizer* ext,
                      const uci::base::Accessor& in,
                      uci::base::Accessor& out) {
    std::string json;
    ext->write(in, json);
    ext->read(json, out);
}

int main() {
    auto* loader = uci_getExternalizerLoader();
    auto* ext = loader->getExternalizer("JSON", "2.5.0", "2.5.0");
    require(ext != nullptr, "getExternalizer returned null");

    auto& serviceIn = uci::type::ServiceStatusMT::create(nullptr);
    serviceIn.getMessageHeader().getSchemaVersion() = "arcal-json-read";
    serviceIn.getMessageData().getTimeUp().setValue(42);
    serviceIn.getMessageData().setServiceState(uci::type::ServiceStateEnum::NORMAL);
    auto& serviceOut = uci::type::ServiceStatusMT::create(nullptr);
    roundtrip(ext, serviceIn, serviceOut);
    require(serviceOut.getMessageHeader().getSchemaVersion() == "arcal-json-read",
            "required nested string round-trips");
    require(serviceOut.getMessageData().getTimeUp().getValue() == 42,
            "required simple restriction round-trips");

    auto& subsystemIn = uci::type::SubsystemStatusMDT::create(nullptr);
    subsystemIn.enableEraseStatus().setValue(uci::type::ProcessingStatusEnum::COMPLETED);
    auto& subsystemOut = uci::type::SubsystemStatusMDT::create(nullptr);
    roundtrip(ext, subsystemIn, subsystemOut);
    require(subsystemOut.hasEraseStatus(), "optional enum present after round-trip");
    require(subsystemOut.getEraseStatus().getValue() == uci::type::ProcessingStatusEnum::COMPLETED,
            "optional enum value round-trips");

    auto& state1 = uci::type::SubsystemStateEnum::create(nullptr);
    auto& state2 = uci::type::SubsystemStateEnum::create(nullptr);
    state1.setValue(uci::type::SubsystemStateEnum::STANDBY);
    state2.setValue(uci::type::SubsystemStateEnum::OPERATE);
    subsystemIn.getCommandableSubsystemState().push_back(state1);
    subsystemIn.getCommandableSubsystemState().push_back(state2);
    roundtrip(ext, subsystemIn, subsystemOut);
    require(subsystemOut.getCommandableSubsystemState().size() == 2,
            "bounded enum list size round-trips");
    require(subsystemOut.getCommandableSubsystemState()[0].getValue() == uci::type::SubsystemStateEnum::STANDBY,
            "bounded enum list first value round-trips");
    require(subsystemOut.getCommandableSubsystemState()[1].getValue() == uci::type::SubsystemStateEnum::OPERATE,
            "bounded enum list second value round-trips");

    auto& choiceIn = uci::type::NameValuePairValueType::create(nullptr);
    choiceIn.chooseDoubleValue() = 42.5;
    auto& choiceOut = uci::type::NameValuePairValueType::create(nullptr);
    std::string choiceJson;
    ext->write(choiceIn, choiceJson);
    std::vector<uint8_t> choiceBytes(choiceJson.begin(), choiceJson.end());
    ext->read(choiceBytes, choiceOut);
    require(choiceOut.isDoubleValue(), "choice variant selected after vector read");
    require(choiceOut.getDoubleValue() == 42.5, "choice value round-trips");

    std::istringstream stream{choiceJson};
    auto& streamOut = uci::type::NameValuePairValueType::create(nullptr);
    ext->read(stream, streamOut);
    require(streamOut.isDoubleValue(), "choice variant selected after stream read");

    loader->destroyExternalizer(ext);
    uci_destroyExternalizerLoader(loader);
    uci::type::NameValuePairValueType::destroy(streamOut);
    uci::type::NameValuePairValueType::destroy(choiceOut);
    uci::type::NameValuePairValueType::destroy(choiceIn);
    uci::type::SubsystemStateEnum::destroy(state2);
    uci::type::SubsystemStateEnum::destroy(state1);
    uci::type::SubsystemStatusMDT::destroy(subsystemOut);
    uci::type::SubsystemStatusMDT::destroy(subsystemIn);
    uci::type::ServiceStatusMT::destroy(serviceOut);
    uci::type::ServiceStatusMT::destroy(serviceIn);

    std::cout << "PASS json_read_roundtrip\n";
    return 0;
}
