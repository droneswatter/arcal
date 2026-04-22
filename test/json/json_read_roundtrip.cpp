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

    uci::type::ActionCommandMT actionIn;
    actionIn.getMessageHeader().getSchemaVersion().setValue("arcal-json-read");
    uci::type::ActionCommandMT actionOut;
    roundtrip(ext, actionIn, actionOut);
    require(actionOut.getMessageHeader().getSchemaVersion().getValue() == "arcal-json-read",
            "required nested string round-trips");

    uci::type::AccessAssessmentMT assessmentIn;
    assessmentIn.enableObjectState().setValue(uci::type::ObjectStateEnum::UPDATED);
    uci::type::AccessAssessmentMT assessmentOut;
    roundtrip(ext, assessmentIn, assessmentOut);
    require(assessmentOut.hasObjectState(), "optional enum present after round-trip");
    require(assessmentOut.getObjectState().getValue() == uci::type::ObjectStateEnum::UPDATED,
            "optional enum value round-trips");

    uci::type::OrbitChangeCapabilityType listIn;
    uci::type::OrbitChangeCapabilityEnum cap;
    cap.setValue(uci::type::OrbitChangeCapabilityEnum::SPECIFIC_ORBIT);
    listIn.getCapabilityType().push_back(cap);
    cap.setValue(uci::type::OrbitChangeCapabilityEnum::RENDEZVOUS);
    listIn.getCapabilityType().push_back(cap);
    uci::type::OrbitChangeCapabilityType listOut;
    roundtrip(ext, listIn, listOut);
    require(listOut.getCapabilityType().size() == 2, "bounded enum list size round-trips");
    require(listOut.getCapabilityType()[0].getValue() == uci::type::OrbitChangeCapabilityEnum::SPECIFIC_ORBIT,
            "bounded enum list first value round-trips");
    require(listOut.getCapabilityType()[1].getValue() == uci::type::OrbitChangeCapabilityEnum::RENDEZVOUS,
            "bounded enum list second value round-trips");

    uci::type::AccelerationChoiceType choiceIn;
    choiceIn.chooseAccelerationValue().setValue(42.5);
    uci::type::AccelerationChoiceType choiceOut;
    std::string choiceJson;
    ext->write(choiceIn, choiceJson);
    std::vector<uint8_t> choiceBytes(choiceJson.begin(), choiceJson.end());
    ext->read(choiceBytes, choiceOut);
    require(choiceOut.isAccelerationValue(), "choice variant selected after vector read");
    require(choiceOut.getAccelerationValue().getValue() == 42.5, "choice value round-trips");

    std::istringstream stream{choiceJson};
    uci::type::AccelerationChoiceType streamOut;
    ext->read(stream, streamOut);
    require(streamOut.isAccelerationValue(), "choice variant selected after stream read");

    loader->destroyExternalizer(ext);
    uci_destroyExternalizerLoader(loader);

    std::cout << "PASS json_read_roundtrip\n";
    return 0;
}
