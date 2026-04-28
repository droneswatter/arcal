// CDR round-trip unit tests — no DDS required.
// Covers: optional absent, optional present, bounded list, choice scalar variant,
// choice struct variant (also exercises optionals nested inside a choice).

#include "uci/type/AMTI_SpecificDataType.h"
#include "uci/type/AccelerationChoiceType.h"
#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"

#include <cmath>
#include <iostream>
#include <vector>

static uci::base::Externalizer* cdr() {
    return uci_getExternalizerLoader()->getExternalizer("CDR", "2.5.0", "2.5.0");
}

static bool test_optional_absent() {
    auto& src = uci::type::AMTI_SpecificDataType::create(nullptr); // CapabilityType left unset
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::AMTI_SpecificDataType::create(nullptr);
    cdr()->read(buf, dst);

    if (dst.hasCapabilityType()) {
        std::cerr << "optional_absent: expected absent, got present\n";
        uci::type::AMTI_SpecificDataType::destroy(dst);
        uci::type::AMTI_SpecificDataType::destroy(src);
        return false;
    }
    uci::type::AMTI_SpecificDataType::destroy(dst);
    uci::type::AMTI_SpecificDataType::destroy(src);
    std::cout << "optional_absent — PASS\n";
    return true;
}

static bool test_optional_present() {
    auto& src = uci::type::AMTI_SpecificDataType::create(nullptr);
    src.enableCapabilityType().setValue(uci::type::AMTI_CapabilityEnum::CUED);
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::AMTI_SpecificDataType::create(nullptr);
    cdr()->read(buf, dst);

    if (!dst.hasCapabilityType()) {
        std::cerr << "optional_present: expected present, got absent\n";
        uci::type::AMTI_SpecificDataType::destroy(dst);
        uci::type::AMTI_SpecificDataType::destroy(src);
        return false;
    }
    if (dst.getCapabilityType().getValue() != uci::type::AMTI_CapabilityEnum::CUED) {
        std::cerr << "optional_present: wrong value\n";
        uci::type::AMTI_SpecificDataType::destroy(dst);
        uci::type::AMTI_SpecificDataType::destroy(src);
        return false;
    }
    uci::type::AMTI_SpecificDataType::destroy(dst);
    uci::type::AMTI_SpecificDataType::destroy(src);
    std::cout << "optional_present — PASS\n";
    return true;
}

static bool test_list() {
    using E = uci::type::AMTI_SubCapabilityEnum;
    auto& src = uci::type::AMTI_SpecificDataType::create(nullptr);
    auto& e1 = E::create(nullptr);
    auto& e2 = E::create(nullptr);
    auto& e3 = E::create(nullptr);
    e1.setValue(E::NAS);
    e2.setValue(E::AAS);
    e3.setValue(E::ACTIVE_TRACK_UPDATE);
    src.getSubCapabilityType().push_back(e1);
    src.getSubCapabilityType().push_back(e2);
    src.getSubCapabilityType().push_back(e3);
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::AMTI_SpecificDataType::create(nullptr);
    cdr()->read(buf, dst);

    const auto& lst = dst.getSubCapabilityType();
    if (lst.size() != 3) {
        std::cerr << "list: expected 3, got " << lst.size() << "\n";
        uci::type::AMTI_SpecificDataType::destroy(dst);
        E::destroy(e3);
        E::destroy(e2);
        E::destroy(e1);
        uci::type::AMTI_SpecificDataType::destroy(src);
        return false;
    }
    if (lst[0].getValue() != E::NAS ||
        lst[1].getValue() != E::AAS ||
        lst[2].getValue() != E::ACTIVE_TRACK_UPDATE) {
        std::cerr << "list: element values don't match\n";
        uci::type::AMTI_SpecificDataType::destroy(dst);
        E::destroy(e3);
        E::destroy(e2);
        E::destroy(e1);
        uci::type::AMTI_SpecificDataType::destroy(src);
        return false;
    }
    uci::type::AMTI_SpecificDataType::destroy(dst);
    E::destroy(e3);
    E::destroy(e2);
    E::destroy(e1);
    uci::type::AMTI_SpecificDataType::destroy(src);
    std::cout << "list — PASS\n";
    return true;
}

static bool test_choice_scalar() {
    auto& src = uci::type::AccelerationChoiceType::create(nullptr);
    src.chooseAccelerationValue() = 9.81;
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::AccelerationChoiceType::create(nullptr);
    cdr()->read(buf, dst);

    if (!dst.isAccelerationValue()) {
        std::cerr << "choice_scalar: wrong variant\n";
        uci::type::AccelerationChoiceType::destroy(dst);
        uci::type::AccelerationChoiceType::destroy(src);
        return false;
    }
    if (std::abs(dst.getAccelerationValue() - 9.81) > 1e-9) {
        std::cerr << "choice_scalar: wrong value\n";
        uci::type::AccelerationChoiceType::destroy(dst);
        uci::type::AccelerationChoiceType::destroy(src);
        return false;
    }
    uci::type::AccelerationChoiceType::destroy(dst);
    uci::type::AccelerationChoiceType::destroy(src);
    std::cout << "choice_scalar — PASS\n";
    return true;
}

// Also exercises optionals nested inside a choice variant.
static bool test_choice_struct() {
    auto& src = uci::type::AccelerationChoiceType::create(nullptr);
    auto& range = src.chooseAccelerationValueRange();
    range.enableMinimumAcceleration() = 1.0;
    range.enableMaximumAcceleration() = 5.0;
    std::vector<uint8_t> buf;
    cdr()->write(src, buf);

    auto& dst = uci::type::AccelerationChoiceType::create(nullptr);
    cdr()->read(buf, dst);

    if (!dst.isAccelerationValueRange()) {
        std::cerr << "choice_struct: wrong variant\n";
        uci::type::AccelerationChoiceType::destroy(dst);
        uci::type::AccelerationChoiceType::destroy(src);
        return false;
    }
    const auto& r = dst.getAccelerationValueRange();
    if (!r.hasMinimumAcceleration() || !r.hasMaximumAcceleration()) {
        std::cerr << "choice_struct: optional subfields absent\n";
        uci::type::AccelerationChoiceType::destroy(dst);
        uci::type::AccelerationChoiceType::destroy(src);
        return false;
    }
    if (std::abs(r.getMinimumAcceleration() - 1.0) > 1e-9 ||
        std::abs(r.getMaximumAcceleration() - 5.0) > 1e-9) {
        std::cerr << "choice_struct: subfield values wrong\n";
        uci::type::AccelerationChoiceType::destroy(dst);
        uci::type::AccelerationChoiceType::destroy(src);
        return false;
    }
    uci::type::AccelerationChoiceType::destroy(dst);
    uci::type::AccelerationChoiceType::destroy(src);
    std::cout << "choice_struct — PASS\n";
    return true;
}

int main() {
    bool ok = true;
    ok &= test_optional_absent();
    ok &= test_optional_present();
    ok &= test_list();
    ok &= test_choice_scalar();
    ok &= test_choice_struct();
    return ok ? 0 : 1;
}
