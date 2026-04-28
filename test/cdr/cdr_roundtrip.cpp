// CDR round-trip unit tests — no DDS required.
// Covers optional absent/present, bounded lists, and scalar/simple choice variants.

#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"
#include "uci/type/NameValuePairValueType.h"
#include "uci/type/ProcessingStatusEnum.h"
#include "uci/type/SubsystemStateEnum.h"
#include "uci/type/SubsystemStatusMDT.h"

#include <cmath>
#include <iostream>
#include <vector>

static uci::base::Externalizer* cdr() {
    return uci_getExternalizerLoader()->getExternalizer("CDR", "2.5.0", "2.5.0");
}

static bool test_optional_absent() {
    auto& src = uci::type::SubsystemStatusMDT::create(nullptr);
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::SubsystemStatusMDT::create(nullptr);
    cdr()->read(buf, dst);

    const bool ok = !dst.hasEraseStatus();
    if (!ok) {
        std::cerr << "optional_absent: expected absent, got present\n";
    }
    uci::type::SubsystemStatusMDT::destroy(dst);
    uci::type::SubsystemStatusMDT::destroy(src);
    std::cout << "optional_absent — " << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
}

static bool test_optional_present() {
    auto& src = uci::type::SubsystemStatusMDT::create(nullptr);
    src.enableEraseStatus().setValue(uci::type::ProcessingStatusEnum::COMPLETED);
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::SubsystemStatusMDT::create(nullptr);
    cdr()->read(buf, dst);

    const bool ok = dst.hasEraseStatus() &&
        dst.getEraseStatus().getValue() == uci::type::ProcessingStatusEnum::COMPLETED;
    if (!ok) {
        std::cerr << "optional_present: wrong optional state\n";
    }
    uci::type::SubsystemStatusMDT::destroy(dst);
    uci::type::SubsystemStatusMDT::destroy(src);
    std::cout << "optional_present — " << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
}

static bool test_list() {
    using E = uci::type::SubsystemStateEnum;
    auto& src = uci::type::SubsystemStatusMDT::create(nullptr);
    auto& e1 = E::create(nullptr);
    auto& e2 = E::create(nullptr);
    auto& e3 = E::create(nullptr);
    e1.setValue(E::STANDBY);
    e2.setValue(E::OPERATE);
    e3.setValue(E::DEGRADED);
    src.getCommandableSubsystemState().push_back(e1);
    src.getCommandableSubsystemState().push_back(e2);
    src.getCommandableSubsystemState().push_back(e3);
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::SubsystemStatusMDT::create(nullptr);
    cdr()->read(buf, dst);

    const auto& lst = dst.getCommandableSubsystemState();
    const bool ok = lst.size() == 3 &&
        lst[0].getValue() == E::STANDBY &&
        lst[1].getValue() == E::OPERATE &&
        lst[2].getValue() == E::DEGRADED;
    if (!ok) {
        std::cerr << "list: element values don't match\n";
    }
    uci::type::SubsystemStatusMDT::destroy(dst);
    E::destroy(e3);
    E::destroy(e2);
    E::destroy(e1);
    uci::type::SubsystemStatusMDT::destroy(src);
    std::cout << "list — " << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
}

static bool test_choice_scalar() {
    auto& src = uci::type::NameValuePairValueType::create(nullptr);
    src.chooseDoubleValue() = 9.81;
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::NameValuePairValueType::create(nullptr);
    cdr()->read(buf, dst);

    const bool ok = dst.isDoubleValue() && std::abs(dst.getDoubleValue() - 9.81) <= 1e-9;
    if (!ok) {
        std::cerr << "choice_scalar: wrong variant/value\n";
    }
    uci::type::NameValuePairValueType::destroy(dst);
    uci::type::NameValuePairValueType::destroy(src);
    std::cout << "choice_scalar — " << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
}

static bool test_choice_string() {
    auto& src = uci::type::NameValuePairValueType::create(nullptr);
    src.chooseStringValue() = "cdr-string-choice";
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::NameValuePairValueType::create(nullptr);
    cdr()->read(buf, dst);

    const bool ok = dst.isStringValue() && dst.getStringValue() == "cdr-string-choice";
    if (!ok) {
        std::cerr << "choice_string: wrong variant/value\n";
    }
    uci::type::NameValuePairValueType::destroy(dst);
    uci::type::NameValuePairValueType::destroy(src);
    std::cout << "choice_string — " << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
}

int main() {
    bool ok = true;
    ok &= test_optional_absent();
    ok &= test_optional_present();
    ok &= test_list();
    ok &= test_choice_scalar();
    ok &= test_choice_string();
    return ok ? 0 : 1;
}
