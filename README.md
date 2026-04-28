# arcal — A Reference C++ CAL

`arcal` is a reference implementation of the OMS Critical Abstraction Layer (CAL) C++ binding, as specified in OMSC-SPC-004 and OMSC-SPC-008. It provides a complete, buildable, government-owned starting point for programs that need a standards-conformant CAL without writing one from scratch.

## What it is

The OMS standard defines the CAL as a transport-agnostic pub/sub API that decouples mission-system services from the underlying communications technology. The specification defines the interface but ships no implementation. `arcal` fills that gap.

**Transport:** Eclipse Cyclone DDS (Apache 2.0). DDS pub/sub maps directly onto CAL semantics, and Cyclone DDS is decentralized — no broker, no central manager, peer discovery via SPDP/SEDP over multicast. A two-host test requires only that both hosts are on the same network; no additional infrastructure.

**Wire encoding:** CDR (Common Data Representation, OMG formal/2002-06-51) over opaque DDS topics. One DDS topic per OMS global element type. CDR is implemented directly with no third-party serialization library, keeping the OMS type system sovereign and the encoding swappable via the Externalizer plugin mechanism defined in OMSC-SPC-008.

**Human-readable externalization:** The JSON externalizer plugin provides bidirectional `read()` and `write()` support for generated UCI Accessors. It is useful for debugging, test fixtures, golden files, tooling, and integration points that need inspectable payloads without changing the DDS/CDR transport path.

**Type system:** OMS message schemas are defined as XSD in the UCI repository. `arcal` includes a Python schema compiler (`tools/schema_compiler/compiler.py`) that reads those XSDs and generates a complete set of C++ Accessor classes plus CDR serialization handlers. The generated code is not checked in; it is produced as a build step.

**Namespace mapping:** `https://www.vdl.afrl.af.mil/programs/oam` → `uci`, per OMSC-SPC-008 §4. Additional namespace mappings can be supplied without modifying the compiler.

## Design decisions

**Why DDS?** The CAL's core model — typed topics, pub/sub, QoS policies — is DDS. Other transports (ZeroMQ, shared memory) require bespoke mapping layers. DDS gives Time-Based Filter, Reliability, Lifespan, and History::KeepLast as first-class QoS policies that map one-to-one onto CAL requirements.

**Why opaque topics?** Each topic carries an `arcal_dds::OpaquePayload` envelope (a `sequence<octet>` defined in `src/dds/arcal_payload.idl`) rather than a type-per-message IDL. This keeps the OMS XSD as the single source of truth — there is no parallel IDL schema to maintain and no risk of the DDS type system diverging from the OMS type system. `dds::core::BytesTopicType` was considered but is gated behind `OMG_DDS_X_TYPES_BUILTIN_TOPIC_TYPES_SUPPORT`, which standard CycloneDDS 0.10.x builds do not enable; a single shared IDL type is equivalent and fully portable.

**Why CDR?** It is the native DDS wire format, requires no dependencies, is well-specified, and is efficient for the structured binary payloads OMS messages contain. The Externalizer plugin interface means it can be replaced (e.g. with JSON or Protobuf) without touching application code.

**Why JSON too?** A binary wire format is great for transport but awkward for humans. The generated JSON externalizer walks the same UCI Accessor tree recursively, including inherited fields, optionals, lists, choices, enumerations, and simple type restrictions. That gives programs a standards-shaped text representation for diagnostics and fixtures while preserving the schema compiler as the single source of truth.

**Why a Python schema compiler?** The UCI XSD corpus is large and defines thousands of types. Maintaining hand-written C++ for all of them is not feasible. The compiler keeps generated output correct and consistent. Generated files are excluded from version control.

**Why unity builds?** The CDR handlers generate thousands of small source files. Compiling them individually is slow and memory-intensive. Unity builds batch related files together, reducing compiler invocations and keeping generated-code builds practical on developer machines.

## Prerequisites

### Build-time

| Tool | Version | Purpose |
|------|---------|---------|
| CMake | ≥ 3.21 | Build system |
| Ninja | any | Build backend (recommended) |
| **clang-20** (recommended) or GCC ≥ 13 | C++17 | Compiler |
| Python 3 | ≥ 3.10 | Schema compiler |
| uv | any | Python dependency manager |
| lxml | (via uv) | XSD parsing |
| Jinja2 | (via uv) | Code generation templates |
| CycloneDDS C library | 0.10.5 | DDS core |
| CycloneDDS-CXX binding | 0.10.5 | C++ DDS API + `idlc` IDL compiler |
| nlohmann_json | ≥ 3.12.0 | CAL config parser and bidirectional JSON Externalizer parser |

> **Use clang-20 when available.** The generated CDR codec is large, and clang generally compiles it faster with lower peak memory use than GCC in this project.

Install Python dependencies:
```bash
uv pip install lxml jinja2
```

#### Installing CycloneDDS — option A: build from source

```bash
bash cmake/install-cyclonedds.sh $HOME/.local
```

This builds and installs CycloneDDS 0.10.5 + the C++ binding to `$HOME/.local`. Pass a different prefix to install elsewhere.

#### Installing CycloneDDS — option B: vcpkg

A `vcpkg.json` manifest is included. With vcpkg bootstrapped and `VCPKG_ROOT` set:

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_CXX_COMPILER=clang++-20 \
  -DCMAKE_C_COMPILER=clang-20 \
  -DCMAKE_BUILD_TYPE=Debug \
  -G Ninja
```

vcpkg will automatically install `cyclonedds[idlc]`, `cyclonedds-cxx[idllib]`,
`nlohmann-json`, and the Boost packages used by LA-CAL (`boost-beast` and
`boost-process`) from the manifest before the build starts. No
`CMAKE_PREFIX_PATH` is needed in this path.

### Runtime

| Dependency | Notes |
|-----------|-------|
| CycloneDDS shared libraries | Must be on `LD_LIBRARY_PATH` if installed to a non-standard prefix |
| Built-in CDR codec | Included in `libarcal.so`; used internally by the DDS transport |
| Network multicast | Required for DDS peer discovery on a LAN; loopback works for single-host |

### Build resources

The generated CDR codec is the heaviest part of the build. Peak memory scales with compiler choice, unity batch size, and parallelism. If the build runs out of memory, reduce the job count first, then reduce `ARCAL_UNITY_BATCH_SIZE`.

On WSL2, set a memory limit in `~/.wslconfig` that leaves enough room for parallel C++ compilation:
```ini
[wsl2]
memory=14GB
swap=4GB
```

## Building

```bash
# Clone and initialize the UCI schema submodule
git clone <repo>
cd arcal
git submodule update --init --recursive

# Generate C++ source from XSD schemas
uv run tools/schema_compiler/compiler.py \
    --schema schema/OAC-STD-UCI_V2.5 \
    --out include

# Configure (clang-20 recommended; swap for GCC if clang-20 is unavailable)
cmake -S . -B build \
    -DCMAKE_CXX_COMPILER=clang++-20 \
    -DCMAKE_C_COMPILER=clang-20 \
    -DCMAKE_PREFIX_PATH="$HOME/.local" \
    -DCMAKE_BUILD_TYPE=Debug \
    -G Ninja

# Build
cmake --build build -j8
```

Or use the convenience script:
```bash
bash scripts/build.sh
```

### Build options

| CMake option | Default | Description |
|-------------|---------|-------------|
| `ARCAL_BUILD_TESTS` | `ON` | Build the CERT test suite |
| `ARCAL_BUILD_E2E_TESTS` | `ON` | Build the E2E smoke tests |
| `ARCAL_BUILD_INSTALL_TESTS` | `ON` | Register install-tree smoke tests |
| `ARCAL_BUILD_EXAMPLES` | `OFF` | Build optional example applications |
| `ARCAL_BUILD_LACAL` | `ON` | Build `arlacal-server`, the WebSocket/OWP language-agnostic CAL bridge |
| `ARCAL_UNITY_BATCH_SIZE` | `25` | Source files per unity build batch; increase to reduce compile time at the cost of higher peak memory |
| `ARCAL_SUBSET_CONFIGS` | empty | Semicolon-separated JSON subset configs that build additional installable CAL libraries such as `arcal-simple` |

Example:
```bash
cmake -S . -B build -DARCAL_UNITY_BATCH_SIZE=25 -G Ninja
```

### Subset CALs

Subset CALs let you build a smaller generated library from a selected set of
message types without overwriting the main `arcal` generated tree. Each subset
is generated under `build/subsets/<cal-name>/`, built as its own shared
library, and installed with its own soname and CMake package.

Subset config format:

```json
{
  "cal_name_suffix": "simple",
  "message_types": [
    "AMTI_Capability",
    "AMTI_CapabilityStatus",
    "AMTI_Command",
    "AMTI_CommandStatus",
    "AMTI_Activity"
  ],
  "accessor_types": [
    "ObjectStateEnum"
  ]
}
```

`message_types` is required. Entries may be OMS global element names such as
`ActionCommand` or generated message accessor names such as `ActionCommandMT`.
`accessor_types` is optional and is useful when a consumer directly includes
non-message accessor headers that are not reachable from the chosen messages.

Build the bundled sample subset:

```bash
cmake -S . -B build \
  -DARCAL_SUBSET_CONFIGS=$PWD/config/subsets/arcal-simple.json \
  -G Ninja
cmake --build build --target arcal_simple arcal_simple_externalizer_json -j4
```

After installation, the subset installs alongside the main library as:

```text
libarcal-simple.so
libarcal-simple_externalizer_json.so
```

and exports its own CMake package:

```cmake
find_package(arcal-simple REQUIRED)
target_link_libraries(my_app PRIVATE arcal_simple::arcal_simple)
```

### Running arcal-cert with a subset CAL

The bundled `config/subsets/arcal-busmon-cert.json` starts from the
bus-monitor demo message flow (`ActionCommand`) and adds the extra accessor
roots referenced directly by `arcal-cert` compile checks.

Configure a dedicated build that points `arcal-cert` at the subset CAL target:

```bash
cmake -S . -B build-subset-cert \
  -DCMAKE_CXX_COMPILER=clang++-20 \
  -DCMAKE_C_COMPILER=clang-20 \
  -DCMAKE_PREFIX_PATH="$HOME/.local" \
  -DARCAL_BUILD_INSTALL_TESTS=OFF \
  -DARCAL_BUILD_EXAMPLES=OFF \
  -DARCAL_BUILD_LACAL=OFF \
  -DARCAL_SUBSET_CONFIGS=$PWD/config/subsets/arcal-busmon-cert.json \
  -DARCAL_CERT_CAL_LIB=arcal_busmon_cert \
  -G Ninja
```

Build the subset CAL and the full standalone + `arcal-cert` test suite against it:

```bash
cmake --build build-subset-cert --target arcal_test_suite_all -j4
ctest --test-dir build-subset-cert --output-on-failure
```

If you want to exercise the runtime `CERT` and `E2E` coverage without the
compile-only conformance checks, add `-DARCAL_CERT_BUILD_COMPILE=OFF` at
configure time. This is useful while generator conformance work is still in
progress.

If you only want the injectable `arcal-cert` suite, `arcal_cert_suite_all`
still builds just that narrower target set.

### Cleaning generated code

```bash
bash scripts/clean-generated.sh
```

This removes `include/uci/type/` and `src/generated/`. Re-run the schema compiler to regenerate.

## Installing

The normal downstream path is to build ARCAL once, install it to a prefix, and
consume the exported CMake package from applications, tools, or integration
tests.

Install ARCAL into a prefix with:

```bash
cmake --install build --prefix /tmp/arcal-install
```

The install tree includes:

- `lib/libarcal.so`
- `lib/libarcal_externalizer_json.so`
- `bin/arlacal-server` when `ARCAL_BUILD_LACAL=ON`
- public headers under `include/`
- JSON externalizer headers under `include/arcal/externalizer/json/`
- CMake package files under `lib/cmake/arcal/`
- reference docs under `share/arcal/`
- the localhost Cyclone DDS config under `share/arcal/examples/`
- optional example source under `share/arcal/examples/`

For non-standard prefixes, make sure the dynamic loader can see the installed
shared libraries. `arlacal-server` is installed with an rpath that points at
its sibling `../lib` directory; direct consumers of `libarcal.so` may still
need `LD_LIBRARY_PATH` or an equivalent loader configuration.

Downstream CMake projects can use the installed package with:

```cmake
find_package(arcal CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE arcal::arcal)
```

If the prefix is not in CMake's default search path, configure consumers with:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/tmp/arcal-install
```

## Configuration and use

### CAL identity configuration

`uci_getAbstractServiceBusConnection()` resolves the service name passed by the
application through ARCAL's CAL config. By default, ARCAL does not silently
invent system or service identities. Set `ARCAL_CONFIG` before constructing an
ASBC:

```bash
export ARCAL_CONFIG=/path/to/arcal-config.json
```

For local development, tests, and intentionally config-less experiments, opt
out explicitly:

```bash
export ARCAL_CONFIG=NONE
```

`ARCAL_CONFIG=NONE` preserves ARCAL's deterministic UUID fallback. It is not
the default, because accidentally running without the deployed CAL identity
configuration can make a service appear to be a different system/service than
intended.

The current JSON format is:

```json
{
  "systems": [
    {
      "name": "IntegrationRig",
      "uuid": "11111111-1111-4111-8111-111111111111",
      "components": [
        { "name": "SystemComponent", "uuid": "22222222-2222-4222-8222-222222222222" }
      ],
      "capabilities": [
        { "name": "SystemCapability", "uuid": "33333333-3333-4333-8333-333333333333" }
      ],
      "subsystems": [
        {
          "name": "Payload",
          "uuid": "44444444-4444-4444-8444-444444444444",
          "services": [
            { "name": "Planner", "uuid": "55555555-5555-4555-8555-555555555555" }
          ],
          "components": [
            { "name": "RadarComponent", "uuid": "66666666-6666-4666-8666-666666666666" }
          ],
          "capabilities": [
            { "name": "DetectCapability", "uuid": "77777777-7777-4777-8777-777777777777" }
          ]
        }
      ]
    }
  ]
}
```

If the config contains exactly one system, ARCAL selects it automatically. If it
contains multiple systems, select the active system explicitly:

```bash
export ARCAL_SYSTEM=IntegrationRig
```

Configured mode is strict:

- unknown service names are rejected during ASBC construction
- service names must be unique within the selected system
- duplicate subsystem, component, or capability names in the selected system
  are rejected
- system, service, subsystem, component, and capability UUIDs come from config
- missing, unreadable, or malformed config is an initialization error

Services may be listed directly under a system or under a subsystem. Services
under a subsystem use that subsystem's UUID for `getMySubsystemUUID()`.
System-level services currently have no associated subsystem UUID, so prefer
placing services under a subsystem when application code uses
`getMySubsystemUUID()`.

### Obtaining a CAL connection

```cpp
#include "uci/base/AbstractServiceBusConnection.h"

uci::base::AbstractServiceBusConnection* conn =
    uci_getAbstractServiceBusConnection("MyServiceLabel", "DDS");
// conn->getStatus() == NORMAL when DDS participant is live
```

Free the connection when done:
```cpp
conn->shutdown();
uci_destroyAbstractServiceBusConnection(conn);
```

### Publishing a message

```cpp
#include "uci/type/ActionCommand.h"  // global element header: factories + listener

auto& writer = uci::type::ActionCommandMT::createWriter("ActionCommand", conn);
auto& msg = uci::type::ActionCommandMT::create(conn);

// ... populate msg fields ...
writer.write(msg);

uci::type::ActionCommandMT::destroy(msg);
writer.close();
uci::type::ActionCommandMT::destroyWriter(writer);
```

### Subscribing to a message

Implement the generated typed listener interface declared in the global element header:

```cpp
#include "uci/type/ActionCommand.h"

class MyListener : public uci::type::ActionCommandMT::Listener {
public:
    void handleMessage(const uci::type::ActionCommandMT& msg) override {
        // handle message
    }
};
```

Then register it with a reader:

```cpp
MyListener listener;
auto& reader = uci::type::ActionCommandMT::createReader("ActionCommand", conn);
reader.addListener(listener);

// Remove when done
reader.removeListener(listener);
reader.close();
uci::type::ActionCommandMT::destroyReader(reader);
```

Polling mode (no listener required):
```cpp
MyListener listener;
unsigned long n = reader.read(/*timeoutMs=*/100, /*maxMessages=*/1, listener);
// listener.handleMessage() was called n times
```

### Recommended sample app

There are two richer samples:

- `examples/amti_service_demo` keeps the raw CxxCAL lifecycle visible:
  `T::create(asb)`, `T::destroy(accessor)`, concrete listener classes, and
  explicit reader/writer cleanup.
- `examples/smti_service_demo` demonstrates the optional `uci::utils` C++
  helpers: `makeMessage<T>()`, `ReaderPtr<T>`, `WriterPtr<T>`,
  `FunctionListener<T>`, ID helpers, and enum helpers.

Both samples demonstrate configured CAL identity, configured static capability
UUIDs, dynamic command/activity UUIDs, capability publication, capability
status, command handling, command status, and activity updates.

Build it explicitly:

```bash
cmake -S . -B build -DARCAL_BUILD_EXAMPLES=ON -G Ninja
cmake --build build --target arcal_amti_service_demo arcal_smti_service_demo -j4
```

The samples are not registered with CTest. Their READMEs include traffic
diagrams showing the message flow and UUID ownership:

```bash
examples/amti_service_demo/README.md
examples/smti_service_demo/README.md
```

`uci::utils` is ARCAL-provided convenience API, not part of OMSC-SPC-008. It is
header-only under `include/uci/utils/` and wraps the public generated CxxCAL
interfaces without changing them.

### Using the CDR externalizer

The CDR externalizer is built into `libarcal.so` and is available through the standard loader interface:

```cpp
#include "uci/base/ExternalizerLoader.h"

uci::base::ExternalizerLoader* loader = uci_getExternalizerLoader();
uci::base::Externalizer* ext = loader->getExternalizer("CDR", "2.5.0", "2.5.0");
```

No separate CDR plugin library is required.

### Using the JSON externalizer

The JSON externalizer is built as `arcal_externalizer_json` and is available through the same loader API. It can serialize a generated Accessor to JSON text and read that JSON back into an Accessor of the same type:

```cpp
#include "uci/base/ExternalizerLoader.h"
#include "uci/type/ActionCommandMT.h"

uci::base::ExternalizerLoader* loader = uci_getExternalizerLoader();
uci::base::Externalizer* json = loader->getExternalizer("JSON", "2.5.0", "2.5.0");

auto& msg = uci::type::ActionCommandMT::create(conn);
msg.getMessageHeader().getSchemaVersion().setValue("2.5.0");

std::string text;
json->write(msg, text);

auto& parsed = uci::type::ActionCommandMT::create(conn);
json->read(text, parsed);

uci::type::ActionCommandMT::destroy(parsed);
uci::type::ActionCommandMT::destroy(msg);
loader->destroyExternalizer(json);
uci_destroyExternalizerLoader(loader);
```

JSON `read()` supports `std::string`, `std::istream`, and UTF-8 `std::vector<uint8_t>` inputs. JSON `write()` supports string and stream outputs; binary vector output remains intentionally unsupported.

### Using `arlacal-server`

`arlacal-server` is ARCAL's language-agnostic CAL bridge. It listens to DDS
topics through the normal ARCAL reader path, decodes opaque CDR payloads into
generated Accessors, externalizes them to JSON, and publishes them over a
WebSocket using OMS Web Protocol (OWP).

Build just the bridge:

```bash
cmake --build build --target arlacal-server
```

Launch it:

```bash
export CYCLONEDDS_URI="file://$(pwd)/test/e2e/cyclonedds_localhost.xml"
./build/lacal/arlacal-server --host 127.0.0.1 --port 8766 --domain 0
```

From an installed prefix:

```bash
export ARCAL_PREFIX=/tmp/arcal-install
export CYCLONEDDS_URI="file://$ARCAL_PREFIX/share/arcal/examples/cyclonedds_localhost.xml"
"$ARCAL_PREFIX/bin/arlacal-server" --host 127.0.0.1 --port 8766 --domain 0
```

Command-line options:

- `--host ADDR` — bind address, default `127.0.0.1`
  - `127.0.0.1` accepts only local connections
  - `0.0.0.0` accepts connections on all local interfaces
  - a specific assigned IP accepts connections only on that interface
- `--port PORT` — WebSocket port, default `8766`
- `--domain ID` — Cyclone DDS domain, default `0`

If you want another machine, container, VM, or WSL/Windows peer to connect,
`127.0.0.1` is usually the wrong choice.

On startup the server logs the listening URL and discovered DDS topics:

```text
[arlacal] listening on ws://127.0.0.1:8766 subprotocol=owp domain=0
[arlacal] discovered topic=ActionCommand
```

For protocol details, ARCAL-specific OWP extensions, and fuller launch notes,
see [lacal/README.md](lacal/README.md).

### DDS network configuration

Cyclone DDS reads its runtime configuration from `CYCLONEDDS_URI`. For single-host testing on Linux, set it to the bundled loopback config:
```bash
export CYCLONEDDS_URI="file://$(pwd)/test/e2e/cyclonedds_localhost.xml"
```
This pins participants to the loopback interface with unicast discovery. On WSL2, standard multicast discovery is unreliable and this config is required.

CTest sets `CYCLONEDDS_URI` automatically for the registered CERT and E2E tests. Export it yourself when running test binaries or examples directly.

## Testing

```bash
ctest --test-dir build --output-on-failure
```

Install-tree smoke tests can be run directly with:

```bash
ctest --test-dir build -R "^INSTALL-" --output-on-failure
```

### CERT tests included

| Test ID | Requirement |
|---------|-------------|
| `CAL-016015` | Multiple CAL Clients in one address space get distinct UUIDs |
| `CAL-016366` | Status listener is called immediately on registration with current state |
| `CAL-005201` | Mechanism to obtain an initialized CAL instance |
| `CAL-005202` | Single CAL instance per unique Service + ASB Identifier combination |
| `CAL-005203` | Mechanism to retrieve System/Service/Subsystem/Component/Capability UUIDs |
| `CAL-005204` | Error reported to CAL Client when initialization fails |
| `CAL-005208` | Topic associated with one and only one CAL Message type |
| `CAL-005209` | Client Topics mapped to applicable CAL Topics |
| `CAL-005210` | Independent QoS settings per Client Topic |

### E2E tests included

| Test ID | Description |
|---------|-------------|
| `E2E-ActionCommand-PubSub` | Publisher and subscriber exchange an ActionCommand over live DDS (two processes) |
| `E2E-TopicIsolation` | Messages on separate topics are not delivered to the wrong subscriber (two processes) |
| `E2E-content-fidelity` | Message content survives CAL publish/subscribe delivery |
| `E2E-multi-message` | Multiple sequential messages are delivered in order |
| `E2E-listener-lifecycle` | Listener added and removed mid-stream receives only in-scope messages |
| `E2E-two-writers-one-reader` | Two concurrent writers deliver to a single reader without loss |
| `E2E-two-readers-one-writer` | One writer delivers to two concurrent readers independently |
| `E2E-readnowait-empty` | Non-blocking read on an empty topic returns zero messages immediately |
| `E2E-read-timeout` | Blocking read with timeout returns after the deadline with no messages |

### Measuring build performance

```bash
bash scripts/measure-build.sh --clean-first -j8
```

Reports wall time and peak memory usage.

## Repository structure

```
arcal/
├── CMakeLists.txt
├── cmake/
│   └── install-cyclonedds.sh   # Builds CycloneDDS from source
├── externalizer/
│   ├── cdr/                    # Built-in CDR codec and Externalizer implementation
│   └── json/                   # Bidirectional JSON Externalizer plugin
├── include/
│   └── uci/
│       ├── base/               # Hand-written abstract interfaces
│       └── type/               # Generated Accessor classes (gitignored)
├── lacal/                      # `arlacal-server` WebSocket/OWP bridge
├── schema/                     # Git submodule → UCI XSD files
├── scripts/
│   ├── build.sh
│   ├── clean-generated.sh
│   └── measure-build.sh
├── src/
│   ├── UUID.cpp
│   ├── dds/                    # Cyclone DDS transport implementation
│   └── generated/              # Generated CDR and JSON handlers (gitignored)
├── test/
│   ├── cert/                   # CERT compliance tests
│   └── e2e/                    # E2E smoke tests (pub/sub against live DDS)
└── tools/
    └── schema_compiler/        # XSD → C++ code generator
        └── compiler.py
```

## License

Government Owned. Developed by or for the U.S. Air Force. See `LICENSE` for terms.
Governed by the Open Architecture Collaborative Working Group (OACWG).
Contact: aflcmc.ase.architectures@us.af.mil
