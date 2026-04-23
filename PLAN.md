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
- Bus monitor architecture moved out of `arcal`: `arcal-busmon` now consumes
  LA-CAL WebSocket/OWP JSON instead of linking against private DDS internals.

## P0: LA-CAL Hardening

Make `arlacal-server` reliable enough to be the default bridge for tools.

- Split the large `lacal/src/main.cpp` into protocol, WebSocket session, topic
  monitor, and server modules.
- Add focused unit tests for OWP parsing and error responses.
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

## P1: Installation And Packaging

Make downstream use boring.

- Add CMake `install()` rules for public headers, libraries, and tools.
- Export an `arcalConfig.cmake` package.
- Install externalizer plugins and document runtime search paths.
- Package the tool surface coherently, not just the core library:
  - `libarcal`
  - externalizer plugins
  - `arlacal-server`
  - example runtime configuration/docs for launching the bridge
- Add a `pkg-config` file (`arcal.pc`) for non-CMake consumers.
- Consider a static-library build option (`BUILD_SHARED_LIBS=OFF`) for embedded
  targets.
- Keep the `vcpkg.json` manifest current and consider a first-party vcpkg port.

Verification:

```bash
cmake --install build --prefix /tmp/arcal-install
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
