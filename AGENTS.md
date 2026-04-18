# arcal — Agent Instructions

## What this project is

arcal is a reference C++ implementation of the OMS Critical Abstraction Layer (CAL), backed by Eclipse Cyclone DDS.

Key facts:
- Generated headers (`include/uci/type/`) and CDR handlers (`src/generated/`) are **not checked in** — they are produced by the schema compiler as a build step.
- The UCI XSD schema is pulled in as a git submodule at `schema/`.
- The CDR externalizer is built as a separate shared library (`arcal_externalizer_cdr`).

## Build

Prerequisites: CMake ≥ 3.21, Ninja, **clang-20** (recommended) or GCC ≥ 13 (C++17), Python 3 ≥ 3.10, uv, CycloneDDS 0.10.5 installed to `$HOME/.local`.

> **Use clang-20.** Building `arcal_externalizer_cdr` (5,614 generated files) with clang-20 takes ~136s and ~4 GB peak RAM. GCC takes ~260s and ~7 GB.

```bash
# Install CycloneDDS from source (first time only)
bash cmake/install-cyclonedds.sh $HOME/.local

# Install Python deps
uv pip install lxml jinja2

# Configure + build (clang-20 recommended)
cmake -S . -B build \
  -DCMAKE_CXX_COMPILER=clang++-20 \
  -DCMAKE_C_COMPILER=clang-20 \
  -DCMAKE_PREFIX_PATH="$HOME/.local" \
  -DCMAKE_BUILD_TYPE=Debug \
  -G Ninja
cmake --build build -j4   # use -j4 on ≤12 GB RAM; -j8 on ≥16 GB
```

Use the convenience script for a full clean rebuild:
```bash
bash scripts/build.sh
```

To measure peak memory during a build:
```bash
bash scripts/measure-build-memory.sh
```

### Build options

| CMake option | Default | Notes |
|---|---|---|
| `ARCAL_BUILD_TESTS` | `ON` | CERT test suite |
| `ARCAL_BUILD_E2E_TESTS` | `ON` | E2E smoke tests |
| `ARCAL_UNITY_BATCH_SIZE` | `25` | Files per unity batch — raise to speed up builds at the cost of more peak RAM |

### Memory constraints (WSL2)

Peak build memory at `-j8` is ~12 GB. Set in `~/.wslconfig`:
```ini
[wsl2]
memory=14GB
swap=4GB
```

On a 12 GB machine, use `-j4`. If still OOM, reduce: `-DARCAL_UNITY_BATCH_SIZE=10`.

## Running tests

```bash
# Full suite
ctest --test-dir build --output-on-failure

# E2E tests only
ctest --test-dir build -R E2E --output-on-failure

# On WSL2, single-process E2E tests need localhost DDS config — CTest sets this automatically via ENVIRONMENT property
```

E2E tests live in `test/e2e/`. They are standalone executables returning 0 on pass. The two multi-process tests (ActionCommand pub/sub, TopicIsolation) use shell script wrappers and a `cyclonedds_localhost.xml` config.

## Schema compiler

```bash
uv run tools/schema_compiler/compiler.py \
    --schema schema/OAC-STD-UCI_V2.5 \
    --out include
```

Generates ~6,400 headers into `include/uci/type/` and ~5,600 CDR handler files into `src/generated/`. Runs in ~2 seconds.

To regenerate from scratch:
```bash
bash scripts/clean-generated.sh
uv run tools/schema_compiler/compiler.py --schema schema/OAC-STD-UCI_V2.5 --out include
```

## Key architectural notes

- **One DDS topic per OMS global element.** Topic payload is an opaque `sequence<octet>` (`arcal_payload.idl`), keeping the OMS XSD as the single source of truth — no parallel IDL to maintain.
- **CDR deserialization uses offset-aware `deserialize_at`.** Nested complex-type fields must call `deserialize_at(buf, off, accessor)` (not `deserialize`) so the caller's offset is carried through the buffer. This is enforced in the generated handlers; do not regress it.
- **Base class fields must be serialized first.** The schema compiler emits `lookup(base_type).serialize(obj, buf)` at the start of each `_serialize_impl`, and `lookup(base_type).deserialize_at(buf, off, obj)` at the start of each `_deserialize_impl`. Inherited fields (e.g., `MessageType` → `MessageHeader`, `SecurityInformation`) are carried this way.
- **Default DDS History QoS is KeepLast(1).** Writing multiple messages without reading in between will drop all but the latest. E2E tests that need N messages delivered interleave writes and reads.
- **DDS DataReaderListener and WaitSet must not be mixed on the same DataReader.** `DdsReader` uses a background thread + WaitSet for the polling path and a separate listener mechanism; never attach a `DataReaderListener` to a reader that also uses a WaitSet.
