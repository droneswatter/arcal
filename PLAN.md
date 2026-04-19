# arcal Roadmap

Items are roughly priority-ordered within each section. "Done" items are kept for reference.

---

## Externalizers

| Status | Item | Notes |
|--------|------|-------|
| Done   | CDR externalizer (read + write) | `libarcal_externalizer_cdr.so` |
| Done   | JSON externalizer — write | `libarcal_externalizer_json.so`; schema-compiler-generated handlers; no external deps |
| Later  | JSON externalizer — read | Needs a JSON parser (nlohmann/json or simdjson); schema compiler generates deserializers |
| Later  | XML externalizer (read + write) | Needs an XML lib (pugixml recommended — header-only, fast); matches OMS wire format for external tools |

---

## arcal-busmon

A standalone tool that subscribes to all topics on the bus and renders/records messages.

### Tier 1 — File Logger (MVP)

- Subscribe to every discovered topic
- Decode each message via the type tag (FNV-1a hash → type name → JSON externalizer)
- Write one file per message: `<log-dir>/<topic>/<seq>.json`
- Print discovered topics and running message counts to stdout
- CLI: `arcal-busmon --log-dir ./busmon-out [--domain 0]`

### Tier 2 — TUI (ncurses / ftxui)

- Live topic list with message counts and rates
- Select a topic to see the latest message body (pretty-printed JSON)
- Filter topics by name glob or XPath-like expression on message fields
- Pause / resume recording

### Tier 3 — HTTP Server

- REST API: `GET /topics`, `GET /topics/{name}/messages?limit=N`
- WebSocket feed for live message stream
- Web UI: topic browser, message viewer, field-level filtering, save/download
- Authentication (token-based)

---

## arcal-replay

Replay a log directory (produced by arcal-busmon Tier 1) back onto the bus.

- Reconstruct message timing from file timestamps or an embedded timestamp field
- CLI: `arcal-replay --log-dir ./busmon-out [--speed 1.0] [--domain 0]`
- Requires JSON read externalizer

---

## Mock ASB

An in-process `AbstractServiceBusConnection` that routes messages via `std::deque` instead of DDS.

- Enables unit tests without a running DDS daemon or network config
- Readers and writers created on the same mock ASB exchange messages synchronously
- Useful for CERT tests that currently require `cyclonedds_localhost.xml`

---

## Installation and Packaging

- CMake `install()` rules for headers, `.so` files, and a `arcalConfig.cmake` find-package file
- vcpkg port (`ports/arcal/`) so downstream projects can depend on arcal via vcpkg
- `pkg-config` file (`arcal.pc`) for non-CMake consumers
- Consider a static-library build option (`BUILD_SHARED_LIBS=OFF`) for embedded targets

---

## Schema Reduction (arcal-subset)

The full UCI schema produces 5,614 CDR handlers and a matching set of JSON handlers — most programs use a small fraction of these types. A schema-reduction option lets integrators ship a purpose-built CAL library that is smaller, faster to compile, and uses less runtime memory.

- Pass the schema compiler a message list (`--messages ActionCommandMT,ActionStatusMT,...`) and a library suffix (`--subset-name mission-planner`)
- Compiler walks the type dependency graph for each listed message type and emits only the required handlers
- Output: `libarcal_cdr_mission-planner.so` / `libarcal_json_mission-planner.so` built from the reduced handler set
- The ExternalizerLoader soname map in arcal core stays unchanged; the subset library is a drop-in replacement
- Useful for embedded/constrained targets and for shortening CI build times during development

---

## Schema Tool (arcal-schema)

A CLI that introspects the arcal type registry without a live bus.

- `arcal-schema list` — list all registered UCI type names
- `arcal-schema show <TypeName>` — dump field structure (names, types, optionality, list bounds)
- `arcal-schema validate <file.json> <TypeName>` — validate a JSON file against the type (requires JSON read)
- Useful for integration testing and documentation generation

---

## Python Bindings

pybind11 wrapper around arcal for scripting, REPL-based debugging, and CI test authoring.

- Expose `AbstractServiceBusConnection`, typed `Reader`/`Writer`, and externalizers
- Enable Python-based bus monitor scripts and test fixtures
- Separate CMake option (`ARCAL_BUILD_PYTHON`, default OFF)

---

## Multi-Domain / Partitioned DDS

- Currently hardcoded to DDS domain 0
- Add `domain` parameter to `DdsAbstractServiceBusConnection` constructor
- Support DDS partitions for topic-level access control (OMS Tier 3 isolation)
