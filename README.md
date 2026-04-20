# arcal — A Reference C++ CAL

arcal is a reference implementation of the OMS Critical Abstraction Layer (CAL) C++ binding, as specified in OMSC-SPC-004 and OMSC-SPC-008. It provides a complete, buildable, government-owned starting point for programs that need a standards-conformant CAL without writing one from scratch.

## What it is

The OMS standard defines the CAL as a transport-agnostic pub/sub API that decouples mission-system services from the underlying communications technology. The specification defines the interface but ships no implementation. arcal fills that gap.

**Transport:** Eclipse Cyclone DDS (Apache 2.0). DDS pub/sub maps directly onto CAL semantics, and Cyclone DDS is decentralized — no broker, no central manager, peer discovery via SPDP/SEDP over multicast. A two-host test requires only that both hosts are on the same network; no additional infrastructure.

**Wire encoding:** CDR (Common Data Representation, OMG formal/2002-06-51) over opaque DDS topics. One DDS topic per OMS global element type. CDR is implemented directly with no third-party serialization library, keeping the OMS type system sovereign and the encoding swappable via the Externalizer plugin mechanism defined in OMSC-SPC-008.

**Type system:** OMS message schemas are defined as XSD in the UCI repository. arcal includes a Python schema compiler (`tools/schema_compiler/compiler.py`) that reads those XSDs and generates a complete set of C++ Accessor classes plus CDR serialization handlers. The generated code is not checked in; it is produced as a build step.

**Namespace mapping:** `https://www.vdl.afrl.af.mil/programs/oam` → `uci`, per OMSC-SPC-008 §4. Additional namespace mappings can be supplied without modifying the compiler.

## Design decisions

**Why DDS?** The CAL's core model — typed topics, pub/sub, QoS policies — is DDS. Other transports (ZeroMQ, shared memory) require bespoke mapping layers. DDS gives Time-Based Filter, Reliability, Lifespan, and History::KeepLast as first-class QoS policies that map one-to-one onto CAL requirements.

**Why opaque topics?** Each topic carries an `arcal_dds::OpaquePayload` envelope (a `sequence<octet>` defined in `src/dds/arcal_payload.idl`) rather than a type-per-message IDL. This keeps the OMS XSD as the single source of truth — there is no parallel IDL schema to maintain and no risk of the DDS type system diverging from the OMS type system. `dds::core::BytesTopicType` was considered but is gated behind `OMG_DDS_X_TYPES_BUILTIN_TOPIC_TYPES_SUPPORT`, which standard CycloneDDS 0.10.x builds do not enable; a single shared IDL type is equivalent and fully portable.

**Why CDR?** It is the native DDS wire format, requires no dependencies, is well-specified, and is efficient for the structured binary payloads OMS messages contain. The Externalizer plugin interface means it can be replaced (e.g. with JSON or Protobuf) without touching application code.

**Why a Python schema compiler?** The UCI XSD corpus is ~147k lines across 16 files defining ~5,600 types. Maintaining hand-written C++ for all of them is not feasible. The compiler runs in under 2 seconds and produces correct, consistent output. Generated files are excluded from version control.

**Why unity builds?** The CDR handlers are 5,600+ small source files. Compiling them individually would take tens of minutes. Unity build batches them into groups of 50, reducing compiler invocations by ~100x and cutting build time to under 4 minutes on an 8-core machine.

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

> **Use clang-20.** Building `arcal_externalizer_cdr` (5,614 generated files) with clang-20 takes ~136s and ~4 GB peak RAM. GCC 13 takes ~260s and ~7 GB.

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

vcpkg will automatically install `cyclonedds[idlc]` and `cyclonedds-cxx[idllib]` from the manifest before the build starts. No `CMAKE_PREFIX_PATH` is needed in this path.

### Runtime

| Dependency | Notes |
|-----------|-------|
| CycloneDDS shared libraries | Must be on `LD_LIBRARY_PATH` if installed to a non-standard prefix |
| `libarcal_externalizer_cdr.so` | CDR externalizer plugin; loaded by the CAL at startup |
| Network multicast | Required for DDS peer discovery on a LAN; loopback works for single-host |

### Memory requirements

| Compiler | `-j8` peak RAM | `-j4` peak RAM |
|----------|---------------|---------------|
| clang-20 | ~4 GB | ~2 GB |
| GCC 13 | ~7 GB | ~4 GB |

Peak memory scales with parallelism. On WSL2, set the memory limit in `~/.wslconfig`:
```ini
[wsl2]
memory=14GB
swap=4GB
```

If you still run out of memory, reduce the batch size: `-DARCAL_UNITY_BATCH_SIZE=10`.

## Building

```bash
# Clone and initialize the UCI schema submodule
git clone <repo>
cd arcal
git submodule update --init --recursive

# Generate C++ source from XSD schemas (~2 seconds)
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

# Build (~2.5 minutes with clang-20 on 8 cores)
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
| `ARCAL_UNITY_BATCH_SIZE` | `25` | Source files per unity build batch; increase to reduce compile time at the cost of higher peak memory |

Example:
```bash
cmake -S . -B build -DARCAL_UNITY_BATCH_SIZE=25 -G Ninja
```

### Cleaning generated code

```bash
bash scripts/clean-generated.sh
```

This removes `include/uci/type/` and `src/generated/`. Re-run the schema compiler to regenerate.

## Configuration and use

### Obtaining a CAL connection

```cpp
#include "uci/base/AbstractServiceBusConnection.h"

uci::base::AbstractServiceBusConnection* conn =
    uci_getAbstractServiceBusConnection("MyServiceLabel", "DDS");
// conn->getStatus() == NORMAL when DDS participant is live
```

Free the connection when done:
```cpp
uci_destroyAbstractServiceBusConnection(conn);
```

### Publishing a message

```cpp
#include "uci/type/ActionCommand.h"  // global element header: factories + listener

uci::base::Writer* writer = uci::type::createActionCommandWriter("ActionCommand");

uci::type::ActionCommandMT msg;
// ... populate msg fields ...
writer->write(msg);

uci::type::destroyActionCommandWriter(writer);
```

### Subscribing to a message

Implement the generated typed listener interface declared in the global element header:

```cpp
#include "uci/type/ActionCommand.h"

class MyListener : public uci::type::ActionCommandListener {
public:
    void handleMessage(const uci::type::ActionCommandMT& msg) override {
        // handle message
    }
};
```

Then register it with a reader:

```cpp
MyListener listener;
uci::base::Reader* reader = uci::type::createActionCommandReader("ActionCommand");
reader->addListener(&listener);

// Remove when done
reader->removeListener(&listener);
uci::type::destroyActionCommandReader(reader);
```

Polling mode (no listener required):
```cpp
MyListener listener;
unsigned long n = reader->read(/*timeoutMs=*/100, /*maxMessages=*/1, listener);
// listener.handleMessage() was called n times
```

### Loading the CDR externalizer

The externalizer is a shared library plugin loaded via the standard loader interface:

```cpp
#include "uci/base/ExternalizerLoader.h"

uci::base::ExternalizerLoader* loader = uci_getExternalizerLoader();
uci::base::Externalizer* ext = loader->getExternalizer("CDR", "2.5.0", "2.5.0");
```

Or link directly against `arcal_externalizer_cdr` and the loader is available automatically.

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
| `E2E-content-fidelity` | All scalar fields round-trip correctly through CDR serialization |
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
│   └── cdr/                    # CDR externalizer plugin (shared library)
├── include/
│   └── uci/
│       ├── base/               # Hand-written abstract interfaces
│       └── type/               # Generated Accessor classes (gitignored)
├── schema/                     # Git submodule → UCI XSD files
├── scripts/
│   ├── build.sh
│   ├── clean-generated.sh
│   └── measure-build.sh
├── src/
│   ├── UUID.cpp
│   ├── dds/                    # Cyclone DDS transport implementation
│   └── generated/              # Generated CDR handlers (gitignored)
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
