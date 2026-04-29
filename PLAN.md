# arcal Roadmap

This roadmap tracks work that belongs in the `arcal` implementation repo. The
portable conformance-suite roadmap lives in `test/arcal-cert/PLAN.md`, and the
standalone browser monitor roadmap belongs in `../arcal-busmon`.

## Done

- CDR externalizer read/write support.
- Bidirectional JSON externalizer read/write support.
- Generated JSON serializers and deserializers.
- CERT/E2E conformance suite wired through `test/arcal-cert`.
- LA-CAL server target: `arlacal-server`.
- OWP smoke coverage for LA-CAL startup, `INIT`, `SUB`, `UNSUB`, `XSUB`, and
  `XUNSUB`.
- LA-CAL server split into protocol, WebSocket session, topic monitor, server,
  and entry-point modules.
- Focused unit coverage for protocol helper parsing, token validation, glob
  matching, WebSocket subprotocol matching, and `INFO` JSON shape.
- Bus monitor architecture moved out of `arcal`: `arcal-busmon` now consumes
  LA-CAL WebSocket/OWP JSON instead of linking against private DDS internals.
- CMake install target for public headers, `libarcal`,
  `libarcal_externalizer_json`, `arlacal-server`, CMake package files,
  reference docs, and example Cyclone DDS config.
- Install-tree smoke coverage for downstream `find_package(arcal CONFIG
  REQUIRED)` consumers and installed `arlacal-server`.
- Optional AMTI service demo with configured capability UUIDs, dynamic
  command/activity UUIDs, and message-flow diagrams.
- Header-only `uci::utils` C++ helpers and companion SMTI service demo showing
  RAII message/reader/writer ownership, lambda listeners, ID helpers, and enum
  helpers.
- Strict CAL config UUID resolution with explicit `ARCAL_CONFIG`, optional
  `ARCAL_SYSTEM`, configured system/service/subsystem/component/capability
  UUIDs, duplicate-service validation, unknown-service rejection, and explicit
  `ARCAL_CONFIG=NONE` development fallback.
- CxxCAL generated accessor conformance: generated accessors are
  implementation-owned interfaces with protected lifecycle, factory/copy/destroy
  APIs, abstract-type handling, conforming primitive accessors, and JSON/CDR
  tests updated to use the public accessor surface.
- Conformance traceability through the embedded `arcal-cert` submodule,
  including `CONFORMANCE.md`, CTest labels, portable CERT/E2E tests, and
  implementation-specific tests kept in `arcal/test/*`.
- ASBC destructor wakeup: `~DdsAbstractServiceBusConnection()` now notifies the
  monitor condition variable before joining so destruction does not wait for the
  monitor loop timeout.
- Root-level configured services keep and return a nil subsystem UUID because
  no subsystem association exists.


## P1: Post-Review Follow-Through (April 2025)

Findings from cross-model review of bd99b57 / a2e1faf / 266f334.

**`arcalMakeCdrExternalizer()` is defined but unreachable**
Defined in `src/dds/CdrBridge.cpp` but not declared in
`include/arcal/CdrBridge.h`. No other TU can call it. Became dead code once
the `registerCdrExternalizerFactory` pattern was introduced in bd99b57.
Remove or declare and call it from the ExternalizerLoader path.

**CdrRegistry string-based APIs are vestigial**
`registerHandler()`, `lookup(typeName)`, and `has(typeName)` in CdrRegistry.h
are never called after the tag-dispatch migration. `table_` is always empty at
runtime. Remove them or mark them explicitly as reserved.

**FNV collision check in compiler.py covers only globally-registered types**
`render_cdr_register_all` raises `ValueError` on tag collisions within its
`type_models` list, but embedded/field types also carry `TYPE_TAG` and are
dispatched via `lookupByTag` during composite serialization. Verify that
`type_models` covers all types, not only top-level global elements. The
runtime backstop (`registerByTag` throws on duplicate at static-init time) is
adequate but a compile-time error is better.

## P1: Optional YAML Support for Config File

Pick an open source C++ YAML lib and detect whether it is available.
If so, detect .yml and .yaml extensions and parse them in yaml instead of json.

## P1: C++ Ergonomic Helper Follow-Through

The first header-only `uci::utils` helper set exists. Keep refining optional,
non-spec helper APIs that make ordinary C++ CAL applications less verbose while
preserving the public CxxCAL interfaces and lifecycle.

- Decide whether to add scoped listener examples that store
  `FunctionListener<T>` and `ScopedListener<T>` as member variables.
- Consider small refinements to helper naming after using the SMTI sample as
  the first comparison point.
- Keep AMTI as the raw CxxCAL sample and SMTI as the helper-based sample.
- Avoid domain-specific message builders until repeated application patterns
  make them worth the extra abstraction.

Verification:

```bash
cmake --build build --target arcal_conformance arcal_amti_service_demo arcal_smti_service_demo
ctest --test-dir build -R "^(CONFORM|CONFIG|CDR|JSON)-" --output-on-failure
```

## P0: LA-CAL Hardening

Make `arlacal-server` reliable enough to be the default bridge for tools.

- Add focused interop tests for OWP error responses:
  malformed `INIT`, duplicate subscription IDs, malformed command lines,
  unsupported operations, and disconnect during active session.
- Add a DDS-backed interop test for `XSUBINFO` behavior and standard `MSG`
  traffic after wildcard subscription setup.

Verification:

```bash
cmake --build build --target arlacal-server lacal_owp_smoke_test
ctest --test-dir build -R "^LACAL-" --output-on-failure
```

## P0: Reader History Depth And Burst Delivery

The generated public `createReader()` / `createWriter()` factories currently
construct `DdsReader` / `DdsWriter` with default `CalQos{}`. That default sets
`messageBufferDepth = 1`, which maps to DDS `History::KeepLast(1)` for both
readers and writers. A listener can therefore miss rapidly incoming messages
before the background reader loop drains DDS. Existing E2E tests work around
this by interleaving writes and reads.

- Pick a safer default history depth for ordinary readers/listeners.
- Decide whether writers should use the same default depth or a separate
  write-history default.
- Add a public or configuration-driven way to set per-topic QoS, including
  message buffer depth, without exposing DDS-specific APIs.
- Add E2E burst tests that write multiple `ServiceStatus` messages rapidly and
  verify listener and polling readers receive all messages up to the configured
  depth.
- Update existing tests and docs to stop relying on interleaved write/read as
  the only reliable multi-message path.

Verification:

```bash
ctest --test-dir build -R "E2E-(multi-message|listener|two-writers)" --output-on-failure
```

## P1: Packaging Follow-Through

The basic CMake install path and smoke coverage exist. Keep rounding out the
packaging surface for non-CMake consumers and release artifacts.

- Decide whether externalizer plugins need a dedicated runtime search path or
  registry convention beyond the current installed shared libraries.
- Add a `pkg-config` file (`arcal.pc`) for non-CMake consumers.
- Consider a static-library build option (`BUILD_SHARED_LIBS=OFF`) for embedded
  targets.
- Consider a tarball/package preset.
- Keep the `vcpkg.json` manifest current and consider a first-party vcpkg port.

Verification:

```bash
cmake --install build --prefix /tmp/arcal-install
ctest --test-dir build -R "^INSTALL-" --output-on-failure
```

## P1: arcal-replay

Replay JSON logs produced by LA-CAL-aware tools back onto the CAL bus.

- Read per-topic JSON logs.
- Reconstruct message timing from file timestamps or embedded timestamp fields.
- Deserialize JSON into generated Accessors with the JSON externalizer.
- Publish through public CAL writers.
- CLI: `arcal-replay --log-dir ./busmon-out [--speed 1.0] [--domain 0]`.

## P1: arcal-schema

Provide schema introspection without a live bus.

- `arcal-schema list` — list registered UCI type names.
- `arcal-schema show <TypeName>` — dump field structure, optionality, list
  bounds, and choice variants.
- `arcal-schema validate <file.json> <TypeName>` — validate JSON against a
  generated Accessor using JSON read support.
- Reuse the generated registries rather than introducing a parallel schema
  model.

## P2: Mock ASB

Add an in-process `AbstractServiceBusConnection` for tests that do not need DDS.

- Route messages via in-memory queues.
- Let readers and writers on the same mock ASB exchange messages synchronously.
- Keep behavior close enough to DDS-backed CAL for CERT-style tests.
- Use it to reduce reliance on `cyclonedds_localhost.xml` where transport
  behavior is not under test.

## P2: Schema Reduction

Allow integrators to build a smaller CAL for a selected message set.

- Pass the schema compiler a message list and subset name.
- Walk the type dependency graph for each listed global message type.
- Emit only required Accessors and externalizer handlers.
- Produce subset libraries with predictable suffixes.
- Keep subset externalizer libraries drop-in compatible with the existing loader
  model.

## P2: Python Bindings

Expose the public CAL API for scripting and test fixtures.

- Wrap `AbstractServiceBusConnection`, typed readers/writers, and externalizers.
- Keep bindings behind `ARCAL_BUILD_PYTHON`, default `OFF`.
- Prefer public APIs over private DDS implementation hooks.

## P2: Multi-Domain / Partitioned DDS

- Keep domain selection configurable for tools and tests.
- Add public API or configuration support if the CAL specification requires it.
- Explore DDS partitions for topic-level access control and OMS isolation
  profiles.
