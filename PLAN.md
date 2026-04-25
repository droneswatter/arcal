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


## P0: CAL Config UUIDs

Add to the CAL Config a list of Systems by Name and UUID.
Each System has a UUID, Services and Subsystems.
Each Subsystem has a UUID, Capabilities and Services.
Each Service has a Name and UUID.
Use one config file that can contain multiple systems. Select the active system
explicitly, for example with `ARCAL_SYSTEM`, unless the config contains exactly
one system. Service names must be unique within the selected system. The ASBC
constructor accepts a Service Name and resolves it within that selected system.

Configured mode is strict:
- If `ARCAL_CONFIG` points to a config file, unknown service names are rejected.
- If multiple systems exist and no active system is selected, initialization is
  rejected.
- If duplicate service names exist in the selected system, config validation
  fails.
- UUIDs for configured systems, services, subsystems, components, and
  capabilities come from config.

Config-less mode is only allowed by explicit opt-out, such as
`ARCAL_CONFIG=NONE`. In that mode ARCAL may keep today's deterministic UUID
fallback for development and tests, but missing or unreadable config must not
silently fall back to config-less behavior.

## P0: Post-Review Fixes (April 2025)

Findings from cross-model review of bd99b57 / a2e1faf / 266f334.

### Must fix

**ASBC destructor stalls up to 2 s per instance**
`~DdsAbstractServiceBusConnection()` sets `running_ = false` but never calls
`monitorCv_.notify_all()`. The monitor thread is blocked in `wait_for(2 s,
predicate)` and will not wake early. The destructor therefore blocks for up to
2 s per instance. The `cal_config_identity` test creates two live ASBC objects
that are destroyed without calling `shutdown()` first — adding ~4 s of
unnecessary latency to that test.
Fix: have the destructor notify the cv (mirroring the pattern already used in
`shutdown()`), or have it call `shutdown()` guarded by `shutdownRequested_`.

### Should fix

**Root-level services silently produce a nil subsystem UUID**
`parseServices(system, …, uci::base::UUID{}, services)` at CalConfig.cpp:157
gives every service declared directly under a `system` (not under a
`subsystem`) a default-constructed UUID as its subsystem UUID. This path is
untested and the nil UUID propagates out of `getMySubsystemUUID()` without
any warning. Decide whether root-level services are spec-valid; if yes,
document the nil UUID in CalConfig.h and add a test; if no, add a validation
error.

**`arcalMakeCdrExternalizer()` is defined but unreachable**
Defined in `src/dds/CdrBridge.cpp` but not declared in
`include/arcal/CdrBridge.h`. No other TU can call it. Became dead code once
the `registerCdrExternalizerFactory` pattern was introduced in bd99b57.
Remove or declare and call it from the ExternalizerLoader path.

### Nice to have

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

**`ARCAL_SYSTEM` selection has no test coverage**
`selectSystem()` has four distinct branches; only the single-system auto-select
path is exercised. Add cases for: (a) multi-system config with correct
`ARCAL_SYSTEM`, (b) multi-system config with no `ARCAL_SYSTEM` (should fail),
(c) `ARCAL_SYSTEM` not present in config (should fail), and (d) config file
path that does not exist.

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

- Add focused unit tests for OWP command handling and error responses.
- Add protocol/interop fixtures for the supported OWP subset:
  - valid `INIT`
  - malformed `INIT`
  - valid `SUB` / `UNSUB`
  - valid `XSUB` / `XUNSUB`
  - duplicate subscription IDs
  - malformed command lines
  - disconnect during active session
- Add tests for `XSUBINFO` behavior and standard `MSG` traffic after wildcard
  subscription setup.
- Add tests for malformed `INIT`, duplicate subscription IDs, and unsupported
  operations.
- Document the supported OWP subset and ARCAL extensions.
- Decide whether topic/message discovery is strictly default-topic based or
  whether DDS-discovered topics can introduce non-default names.

Verification:

```bash
cmake --build build --target arlacal-server lacal_owp_smoke_test
ctest --test-dir build -R "^LACAL-" --output-on-failure
```

## P0: CxxCAL Generated Accessor Conformance

Bring generated `uci::type::*` accessors into the OMSC-SPC-008 RevK lifecycle
and factory model. This is non-negotiable for CAL interchangeability and for
implementation-controlled memory management.

- Treat public generated `uci::type::*` classes as CxxCAL accessor interfaces,
  not application-constructible value objects.
- Move data members into ARCAL-owned generated implementation classes.
- Make generated accessor constructors, copy constructors, assignment
  operators, and destructors protected.
- Add generated `copy(const T&)`, `create(...)`, copy-create, and `destroy(...)`
  for non-abstract complex accessors.
- Parse and model XSD `abstract="true"` so abstract accessors do not expose
  concrete factories.
- Update CDR/JSON generation and ARCAL tests to use factories or implementation
  classes instead of stack-constructed public accessors.
- Track details in [CXX_CAL_SPEC_AUDIT.md](CXX_CAL_SPEC_AUDIT.md).

Verification:

```bash
cmake --build build --target arcal_conformance
ctest --test-dir build -R "^(CONFORM|JSON|CDR|E2E|CERT)-" --output-on-failure
```

## P0: Conformance Traceability

Use `arcal-cert` as the portable requirements surface and keep the embedded
submodule current.

- Add `CONFORMANCE.md` in `arcal-cert`.
- Add CTest labels in `arcal-cert` and verify they appear in the parent build.
- Update `test/arcal-cert` whenever the standalone `arcal-cert` repo advances.
- Keep implementation-specific tests in `arcal/test/*`, not in `arcal-cert`.

Verification:

```bash
ctest --test-dir build -N
ctest --test-dir build -R "^(CERT|E2E)-" --output-on-failure
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
