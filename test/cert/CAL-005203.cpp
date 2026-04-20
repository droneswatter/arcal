// CERT CAL-005203: A CAL Implementation shall have a mechanism for retrieving
// the UUIDs that identify the System, Service, Subsystem, Components, and
// Capabilities associated with the initializing Service.

#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/UUID.h"
#include <cassert>
#include <iostream>

int main() {
    auto* asb = uci_getAbstractServiceBusConnection("UUIDService", "DDS");
    assert(asb);

    auto sys  = asb->getMySystemUUID();
    auto svc  = asb->getMyServiceUUID();
    auto sub  = asb->getMySubsystemUUID();
    auto comp = asb->getMyComponentUUID("RadarComponent");
    auto cap  = asb->getMyCapabilityUUID("DetectionCapability");

    assert(!sys.isNil()  && sys.isValid());
    assert(!svc.isNil()  && svc.isValid());
    assert(!sub.isNil()  && sub.isValid());
    assert(!comp.isNil() && comp.isValid());
    assert(!cap.isNil()  && cap.isValid());

    // UUIDs at different scope levels must be distinct
    assert(sys != svc);
    assert(svc != sub);
    assert(sub != comp);
    assert(comp != cap);

    // Different names at the same scope level must produce different UUIDs
    assert(comp != asb->getMyComponentUUID("SensorComponent"));
    assert(cap  != asb->getMyCapabilityUUID("TrackingCapability"));

    asb->shutdown();
    uci_destroyAbstractServiceBusConnection(asb);
    std::cout << "PASS CAL-005203\n";
    return 0;
}
