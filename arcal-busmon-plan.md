# arcal-busmon Phase 1 — Design Plan

Standalone bus monitor that subscribes to all arcal topics on a DDS domain, decodes messages, and writes them as JSON files. Lives in its own repo (`arcal-busmon/`), sibling to `arcal/`.

---

## CLI

```
arcal-busmon [--log-dir DIR] [--domain ID] [--duration SECONDS]
```

| Flag | Default | Description |
|------|---------|-------------|
| `--log-dir` | `./busmon-out` | Root directory for JSON log files |
| `--domain` | `0` | DDS domain ID |
| `--duration` | (run until Ctrl-C) | Stop after N seconds |

Stdout prints discovered topics and a running message count per topic.

---

## Repo & Build

```
arcal-busmon/
  CMakeLists.txt
  src/
    main.cpp
    TopicMonitor.h/.cpp   # DDS discovery + per-topic raw reader management
    DecodeChain.h/.cpp    # tag → accessor → JSON
    LogWriter.h/.cpp      # file output
  PLAN.md
```

**CMake dependencies** (installed to `~/.local`):
- `arcal` — core CAL library; provides `arcalCreateAccessor`, `cdrDeserialize`, `uci_getExternalizerLoader`
- `arcal_externalizer_cdr` — linked directly so its static initializer fires, registering CDR handlers into arcal core
- `arcal_externalizer_json` — linked directly so its static initializer fires, registering JSON handlers; obtained via `uci_getExternalizerLoader`
- `CycloneDDS` — C API used directly for built-in topic discovery and raw readers

---

## Architecture

### A. Topic Discovery

`DDS_BUILTIN_TOPIC_DCPSPUBLICATION` delivers a sample for every new publisher on the domain. Each sample carries:
- **DDS topic name** — an arbitrary application-defined string (e.g. `"SomeMissionTopic"`); does **not** identify the UCI type
- **DDS type name** — always `"arcal_payload"` for arcal publishers

`TopicMonitor` filters for `type_name == "arcal_payload"` and ignores everything else. For each matching topic, it creates a raw `arcal_payload` DDS reader.

The UCI message type is **not** known from DDS discovery. It is embedded in the first 4 bytes of every payload as a 32-bit FNV-1a tag.

```
DDS participant (domain D)
  └─ reader on DDS_BUILTIN_TOPIC_DCPSPUBLICATION
       → on new publication: filter type_name == "arcal_payload"
       → extract DDS topic_name (for log directory naming)
       → create raw arcal_payload DDS reader for that topic
       → pass reader to DecodeChain
```

### B. Required arcal change — Accessor Factory

The CDR deserialization functions in arcal core (`cdrDeserialize`) operate on `uci::base::Accessor&`. busmon must instantiate the correct concrete type before deserializing. This requires a factory keyed by the 32-bit type tag (not a string — the tag is what busmon has off the wire):

```
uint32_t tag  →  uci::base::Accessor*   (heap-allocated, caller owns)
```

**Only global element types (~760 message types) are registered** — these are the only UCI types that ever appear as top-level tagged payloads on the DDS bus. Sub-types are embedded within message payloads and are handled internally by the CDR/JSON handlers.

arcal does not currently expose this. The fix is entirely within arcal core:

1. **`include/arcal/AccessorFactory.h`** — public header declaring `arcalCreateAccessor(uint32_t tag)` and `arcalDestroyAccessor(Accessor*)`
2. **Schema compiler** — generate `src/generated/accessor_factory_all.cpp` alongside `factories_all.cpp`; same 760 message-type includes, switch on tag → `new uci::type::SomeType{}`
3. **`CMakeLists.txt`** — add the generated file to arcal's source list (same approach as `factories_all.cpp`; single TU, no unity batching)

The display name (`"ActionCommandMT"`) is obtained from `accessor->typeName()` after creation — no separate tag→string lookup needed.

### C. Decode Pipeline

For each payload received on a raw `arcal_payload` reader:

```
raw bytes (sequence<octet>)
  ├─ tag      = bytes[0..3] as uint32_t LE
  ├─ accessor = arcalCreateAccessor(tag)          // nullptr → drop, log to stderr
  ├─ arcal::cdrDeserialize(bytes, *accessor)       // strips 4-byte header, fills accessor
  ├─ ext->write(*accessor, json_str)               // JsonExternalizer via ExternalizerLoader
  ├─ LogWriter::write(topic_name, seq, json_str)
  └─ arcalDestroyAccessor(accessor)
```

`ext` is a `uci::base::Externalizer*` obtained once at startup via `uci_getExternalizerLoader()->getExternalizer("JSON", ...)`. Neither arcal core nor `arcal_externalizer_cdr` touch JSON.

### D. Log File Output

```
<log-dir>/
  <topic-name>/
    0000000001.json
    0000000002.json
    ...
```

Topic names sanitized for filesystem use (replace `/` and `:` with `_`). Sequence numbers zero-padded to 10 digits. No pretty-printing in phase 1.

Stdout per message:
```
[ActionCommandMT]  topic=SomeTopic  seq=1  bytes=342  tag=0xA3F2B1C4
```

---

## Sequencing

### Step 1 — arcal changes (prerequisite, in arcal repo)
- [ ] Add `include/arcal/AccessorFactory.h`
- [ ] Schema compiler: generate `accessor_factory_all.cpp` (global elements only, keyed by tag)
- [ ] Add `src/generated/accessor_factory_all.cpp` to arcal CMakeLists
- [ ] Rebuild and run tests; commit

### Step 2 — arcal-busmon skeleton (new repo)
- [ ] Init repo, CMakeLists.txt linking arcal + both externalizers + CycloneDDS C API
- [ ] `main.cpp`: arg parsing, DDS participant, `uci_getExternalizerLoader` setup, run loop, Ctrl-C
- [ ] `LogWriter`: directory creation, file naming, write + flush

### Step 3 — discovery + raw reading
- [ ] `TopicMonitor`: built-in reader on `DDS_BUILTIN_TOPIC_DCPSPUBLICATION`
- [ ] Per-topic raw `arcal_payload` DDS reader creation via C API
- [ ] Single `dds_waitset` across all readers (add dynamically as topics are discovered)

### Step 4 — decode chain + integration test
- [ ] `DecodeChain`: tag extraction, `arcalCreateAccessor`, `cdrDeserialize`, `ext->write`
- [ ] Graceful handling of unknown tags
- [ ] Integration test: run arcal `e2e_pub`, verify busmon produces expected JSON file

---

## Open Questions

1. **WaitSet vs per-thread**: a single `dds_waitset` is preferred — simpler lifecycle, no thread-per-topic explosion. Readers are attached to it dynamically as topics are discovered.

2. **Log file rollover**: phase 1 has no limit. Phase 2 adds `--max-files N`.

3. **Timestamp**: should busmon inject a `_busmon_ts` (wall-clock ISO-8601) into each JSON file for replay purposes? Deferred to phase 2.
