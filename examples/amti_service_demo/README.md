# AMTI Service Demo

This sample demonstrates the CAL message choreography for a small AMTI service
using the raw generated CxxCAL API. It intentionally keeps `T::create(asb)`,
`T::destroy(accessor)`, concrete listener classes, and explicit reader/writer
cleanup visible. It also shows the direct ASBC lifecycle:
`uci_getAbstractServiceBusConnection(...)`, `shutdown()`, and
`uci_destroyAbstractServiceBusConnection(...)`.

It is documentation-grade: the AMTI payloads are intentionally skeletal, while
the identity, UUID, topic, command, status, and activity flow are explicit.

For the same style of flow using ARCAL's optional C++ helper layer, compare this
sample with `examples/smti_service_demo`.

The sample uses configured CAL identity. Do not run it with `ARCAL_CONFIG=NONE`.

## Build

```bash
cmake -S . -B build \
  -DARCAL_BUILD_EXAMPLES=ON \
  -DCMAKE_PREFIX_PATH="$HOME/.local" \
  -G Ninja

cmake --build build --target arcal_amti_service_demo -j4
```

## Run

In both terminals:

```bash
export ARCAL_CONFIG="$PWD/examples/amti_service_demo/arcal-config.json"
export CYCLONEDDS_URI="file://$PWD/test/e2e/cyclonedds_localhost.xml"
```

Terminal 1:

```bash
./build/examples/amti_service_demo/arcal_amti_service_demo service
```

Terminal 2:

```bash
./build/examples/amti_service_demo/arcal_amti_service_demo client --demo
```

The service exits after it accepts a start command, publishes an active
activity, accepts a stop command for that activity, and publishes the completed
activity update.

## UUID Lifecycle

| UUID | Source | Owner | Carried by |
|------|--------|-------|------------|
| `systemUUID` | configured | CAL config | ASBC identity |
| `subsystemUUID` | configured | CAL config | ASBC identity, `AMTI_ActivityMT` subsystem ID |
| `AmtiSensorService` UUID | configured | CAL config | service ASBC identity |
| `AmtiOperatorService` UUID | configured | CAL config | client ASBC identity |
| `capabilityUUID` | configured | CAL config | `AMTI_CapabilityMT`, `AMTI_CapabilityStatusMT`, start `AMTI_CommandMT`, `AMTI_ActivityMT` |
| `startCommandUUID` | dynamic | client | start `AMTI_CommandMT`, matching `AMTI_CommandStatusMT` |
| `activityUUID` | dynamic | service | start `AMTI_CommandStatusMT`, `AMTI_ActivityMT`, stop `AMTI_CommandMT` |
| `stopCommandUUID` | dynamic | client | stop `AMTI_CommandMT`, matching `AMTI_CommandStatusMT` |

## Startup And Capability Advertisement

```mermaid
sequenceDiagram
    participant Config as arcal-config.json
    participant Service as AmtiSensorService
    participant Bus as CAL/DDS Topics
    participant Client as AmtiOperatorService

    Config-->>Service: systemUUID, subsystemUUID, serviceUUID [configured]
    Config-->>Service: capabilityUUID [configured]
    Config-->>Client: systemUUID, subsystemUUID, serviceUUID [configured]
    Service->>Bus: AMTI_CapabilityMT(capabilityUUID [configured])
    Service->>Bus: AMTI_CapabilityStatusMT(capabilityUUID [configured], AVAILABLE)
```

## Start Command Flow

```mermaid
sequenceDiagram
    participant Client as AmtiOperatorService
    participant Bus as CAL/DDS Topics
    participant Service as AmtiSensorService

    Client->>Client: create startCommandUUID [dynamic]
    Client->>Bus: AMTI_CommandMT(startCommandUUID [dynamic], capabilityUUID [configured])
    Bus->>Service: AMTI_CommandMT
    Service->>Service: create activityUUID [dynamic]
    Service->>Bus: AMTI_CommandStatusMT(startCommandUUID [dynamic], ACCEPTED, activityUUID [dynamic])
    Service->>Bus: AMTI_ActivityMT(activityUUID [dynamic], capabilityUUID [configured], ACTIVE_UNCONSTRAINED)
    Bus->>Client: AMTI_CommandStatusMT + AMTI_ActivityMT
```

## Stop Command Flow

```mermaid
sequenceDiagram
    participant Client as AmtiOperatorService
    participant Bus as CAL/DDS Topics
    participant Service as AmtiSensorService

    Client->>Client: create stopCommandUUID [dynamic]
    Client->>Bus: AMTI_CommandMT(stopCommandUUID [dynamic], activityUUID [dynamic], DISABLE)
    Bus->>Service: AMTI_CommandMT
    Service->>Bus: AMTI_CommandStatusMT(stopCommandUUID [dynamic], ACCEPTED)
    Service->>Bus: AMTI_ActivityMT(activityUUID [dynamic], capabilityUUID [configured], COMPLETED)
    Bus->>Client: AMTI_CommandStatusMT + AMTI_ActivityMT
```

## Notes

- The AMTI mission/radar details are intentionally sparse. This sample teaches
  CAL message flow and identity handling, not operational AMTI modeling.
- DDS discovery can take a moment on process startup. Start the service first,
  then run the client.
- The sample is not registered with CTest. It is built only when
  `ARCAL_BUILD_EXAMPLES=ON`.
