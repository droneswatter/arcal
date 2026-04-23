# arlacal-server

`arlacal-server` is ARCAL's language-agnostic CAL bridge. It listens to DDS
topics through the normal ARCAL reader path, decodes opaque CDR payloads into
generated Accessors, externalizes them to JSON, and publishes them over a
WebSocket using OMS Web Protocol (OWP).

It is mainly intended as a debugging and integration bridge for non-C++ tools
and UI clients.

## Build

`arlacal-server` is built when `ARCAL_BUILD_LACAL=ON` (the default).

Build just the bridge:

```bash
cmake --build build --target arlacal-server
```

The executable is written to:

```text
build/lacal/arlacal-server
```

## Launch

For single-host development on Linux or WSL, use ARCAL's bundled loopback
Cyclone DDS configuration:

```bash
export CYCLONEDDS_URI="file://$(pwd)/test/e2e/cyclonedds_localhost.xml"
./build/lacal/arlacal-server --host 127.0.0.1 --port 8766 --domain 0
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

## Connection Model

Clients connect over WebSocket and must negotiate:

- subprotocol: `owp`

If the client does not request `owp`, the server rejects the WebSocket upgrade.

One `arlacal-server` process bridges one DDS domain. JSON payloads are produced
through `arcal_externalizer_json`.

## OWP Handshake

The first protocol operation must be `INIT`, for example:

```text
INIT {"versions":["1.0"],"schema":"2.5.0","verbose":true,"service_id":"tooling-client"}
```

Required `INIT` fields:

- `versions`
- `schema`
- `service_id`

Current supported version:

- `1.0`

If `INIT` succeeds, the server replies with:

- `INFO <json>`
- `+OK` when `verbose` is `true`

If `INIT` is malformed or no supported version is offered, the server replies
with `-ERR ...`.

The `INFO` payload currently includes:

- `version`
- `server_id`
- `system_label`
- `service_id`
- `connect_urls`
- `extensions`
- `uuids`

Today `extensions` includes:

- `arcal.xsub.v1`

## Supported Commands

Standard OWP-style commands currently implemented:

- `INIT <json>`
- `SUB <subscription-id> <message-name> <topic> [group]`
- `UNSUB <subscription-id>`

ARCAL extension commands:

- `XSUB <stream-id> <topic-pattern> [message-pattern]`
- `XUNSUB <stream-id>`

Current response forms:

- `+OK`
- `-ERR <code> <detail>`
- `INFO <json>`
- `MSG <subscription-id> <json>`
- `XSUBINFO <stream-id> <subscription-id> <topic> <message-name>`

Unsupported or out-of-order operations are rejected with `-ERR`.

Notably, the current server does **not** implement publish-side OWP commands.
For example:

- `PUB` is rejected with `-ERR Illegal-State operation not implemented in this arlacal-server build`
- unknown operations are rejected with `-ERR Illegal-Argument unknown operation`

## ARCAL Extension: Wildcard Subscription

`XSUB` is the bridge-specific convenience extension used by tools such as
`arcal-busmon`.

Behavior:

1. The client sends one wildcard request:

```text
XSUB busmon *
```

2. When a DDS topic/message pair first matches that wildcard, the server sends:

```text
XSUBINFO <stream-id> <subscription-id> <topic> <message-name>
```

3. After that, ordinary traffic uses standard `MSG` lines:

```text
MSG <subscription-id> <json>
```

The server generates the concrete `subscription-id` values for
wildcard-derived subscriptions. This keeps high-volume message traffic on
ordinary `MSG` responses while still giving the client enough metadata to map a
subscription ID back to `<topic, message-name>`.

`XUNSUB <stream-id>` removes the wildcard stream and its server-generated
concrete subscriptions.

## Topic Model

The server watches DDS publication discovery and creates readers for ARCAL's
opaque payload topics as they appear on the bus.

Current assumptions:

- discovered DDS topics are bridged as they appear
- wildcard/discovery behavior is mainly a tooling convenience
- deployed services are still expected to know their topic names ahead of time

## Current Scope

`arlacal-server` is a bridge for debugging and integration. It is not yet trying
to expose every CAL behavior over OWP.

In particular, the current implementation is centered on:

- receiving DDS traffic
- decoding CDR payloads into generated Accessors
- externalizing those Accessors to JSON
- forwarding JSON payloads over WebSocket/OWP

See also:

- [../README.md](../README.md) for the main ARCAL build and runtime guide
- [test/owp_smoke_test.cpp](test/owp_smoke_test.cpp) for a focused OWP protocol
  smoke test
