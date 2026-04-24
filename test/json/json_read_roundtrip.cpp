// JSON externalizer — read/write round-trip coverage.

#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/type/AccelerationChoiceType.h"
#include "uci/type/AccessAssessmentMT.h"
#include "uci/type/ActionCommandMT.h"
#include "uci/type/OrbitChangeCapabilityType.h"

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

    auto& actionIn = uci::type::ActionCommandMT::create(nullptr);
    actionIn.getMessageHeader().getSchemaVersion().setValue("arcal-json-read");
    auto& actionOut = uci::type::ActionCommandMT::create(nullptr);
    roundtrip(ext, actionIn, actionOut);
    require(actionOut.getMessageHeader().getSchemaVersion().getValue() == "arcal-json-read",
            "required nested string round-trips");

    auto& assessmentIn = uci::type::AccessAssessmentMT::create(nullptr);
    assessmentIn.enableObjectState().setValue(uci::type::ObjectStateEnum::UPDATED);
    auto& assessmentOut = uci::type::AccessAssessmentMT::create(nullptr);
    roundtrip(ext, assessmentIn, assessmentOut);
    require(assessmentOut.hasObjectState(), "optional enum present after round-trip");
    require(assessmentOut.getObjectState().getValue() == uci::type::ObjectStateEnum::UPDATED,
            "optional enum value round-trips");

    auto& listIn = uci::type::OrbitChangeCapabilityType::create(nullptr);
    auto& cap = uci::type::OrbitChangeCapabilityEnum::create(nullptr);
    cap.setValue(uci::type::OrbitChangeCapabilityEnum::SPECIFIC_ORBIT);
    listIn.getCapabilityType().push_back(cap);
    cap.setValue(uci::type::OrbitChangeCapabilityEnum::RENDEZVOUS);
    listIn.getCapabilityType().push_back(cap);
    auto& listOut = uci::type::OrbitChangeCapabilityType::create(nullptr);
    roundtrip(ext, listIn, listOut);
    require(listOut.getCapabilityType().size() == 2, "bounded enum list size round-trips");
    require(listOut.getCapabilityType()[0].getValue() == uci::type::OrbitChangeCapabilityEnum::SPECIFIC_ORBIT,
            "bounded enum list first value round-trips");
    require(listOut.getCapabilityType()[1].getValue() == uci::type::OrbitChangeCapabilityEnum::RENDEZVOUS,
            "bounded enum list second value round-trips");

    auto& choiceIn = uci::type::AccelerationChoiceType::create(nullptr);
    choiceIn.chooseAccelerationValue().setValue(42.5);
    auto& choiceOut = uci::type::AccelerationChoiceType::create(nullptr);
    std::string choiceJson;
    ext->write(choiceIn, choiceJson);
    std::vector<uint8_t> choiceBytes(choiceJson.begin(), choiceJson.end());
    ext->read(choiceBytes, choiceOut);
    require(choiceOut.isAccelerationValue(), "choice variant selected after vector read");
    require(choiceOut.getAccelerationValue().getValue() == 42.5, "choice value round-trips");

    std::istringstream stream{choiceJson};
    auto& streamOut = uci::type::AccelerationChoiceType::create(nullptr);
    ext->read(stream, streamOut);
    require(streamOut.isAccelerationValue(), "choice variant selected after stream read");

    loader->destroyExternalizer(ext);
    uci_destroyExternalizerLoader(loader);
    uci::type::AccelerationChoiceType::destroy(streamOut);
    uci::type::AccelerationChoiceType::destroy(choiceOut);
    uci::type::AccelerationChoiceType::destroy(choiceIn);
    uci::type::OrbitChangeCapabilityType::destroy(listOut);
    uci::type::OrbitChangeCapabilityEnum::destroy(cap);
    uci::type::OrbitChangeCapabilityType::destroy(listIn);
    uci::type::AccessAssessmentMT::destroy(assessmentOut);
    uci::type::AccessAssessmentMT::destroy(assessmentIn);
    uci::type::ActionCommandMT::destroy(actionOut);
    uci::type::ActionCommandMT::destroy(actionIn);

    std::cout << "PASS json_read_roundtrip\n";
    return 0;
}
