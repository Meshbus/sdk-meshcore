# MeshCore C Library Architecture

This document describes the current architecture of this MeshCore C library. The
implementation is organized into core, support, runtime, and platform-boundary
responsibility areas, with upstream MeshCore used as compatibility evidence.

The library is a platform-neutral C MeshCore protocol engine with four physical
areas:

1. `src/core/`: 1:1 API-design translation from upstream top-level protocol
   classes
2. `src/support/`: explicitly promoted helper/support modules
3. `src/runtime/`: upstream-compatible behavior extracted from runtime helpers
   and examples
4. `src/platform/`: runtime dispatch bridge for the explicit platform hook
   contract

`UPSTREAM.md` records the evidence classification for these areas.

## Design Goals

- Preserve LoRa wire compatibility with upstream Arduino MeshCore.
- Make upstream protocol surfaces traceable at the C API-design level.
- Keep Arduino scheduling and platform code out of the generic library.
- Let hosts own scheduling, persistence, transport, and hardware integration.
- Make future upstream updates reviewable by layer.
- Keep implementation and tests reviewable by layer.

## Non-Goals

- Do not port Arduino `loop()` as the C runtime model.
- Do not make concrete host services, Bluetooth, storage, shell, board, or
  sample code part of the generic library.

## Layering

```text
Host application or service
  owns scheduling, storage, transport, board integration

Public include boundary
  meshcore/types.h
  meshcore/runtime.h
  meshcore/platform.h

src/runtime/
  typed requests
  radio RX/TX event ingress
  timer event ingress
  pending request lifecycle
  publish callbacks
  direct platform hook dispatch
  behavior extracted from upstream runtime helpers and examples

src/core/
  packet
  identity
  utils
  rng
  clock
  radio abstraction
  dispatcher
  mesh
  group channel

src/support/
  packet manager
  tables
  advert data
  telemetry compatibility
  crypto support

src/platform/
  direct platform hook dispatch bridge
```

Layer names are implementation responsibilities. Move files when their
responsibility changes, with checks for the affected behavior and boundary.

## Source Manifest

`cmake/meshcore_sources.cmake` is the canonical source manifest
for library implementation files. Full-runtime consumers such as downstream
host adapters and runtime oracle tests must use
`MESHCORE_RUNTIME_LIBRARY_SOURCES` instead of maintaining private complete
source lists.

Focused protocol tests may use smaller manifest groups, such as
`MESHCORE_PROTOCOL_PACKET_SOURCES` or `MESHCORE_SUPPORT_ADVERT_DATA_SOURCES`,
so they remain module-level tests. New `.c` implementation files under `src`
must be added to the manifest or explicitly moved outside the generic
implementation root.

## Sync Observability

`upstream.lock` records the expected `.reference/meshcore` checkout.
`tools/upstream_lock_check.py` verifies the checkout commit and dirty state.
`tools/meshcore_sync_report.py` aggregates the lock check, evidence-file
presence, source manifest coverage, public-header ownership, include-boundary
scans, stale source-root scans, and runtime test-hook boundary checks.

Run the sync report before changing upstream evidence classification or moving
architecture boundaries. Treat failures as actionable drift that must be
classified before implementation continues.

## Layer 1: Core And Support

`src/core/` is the 1:1 traceability layer against top-level
`.reference/meshcore/src` classes. `src/support/` contains helper-derived
modules that are intentionally promoted into the generic library but remain
separate from protocol core.

The term "1:1" means API-design traceability, not mechanical C++ cloning. C may
use explicit context pointers, fixed buffers, result codes, and callback-free
helpers where C++ used classes, inheritance, or Arduino types.

The following upstream surfaces are Layer 1 inputs.

| Target protocol module | Upstream API evidence | Notes |
| --- | --- | --- |
| `packet` | `.reference/meshcore/src/Packet.h`, `.reference/meshcore/src/Packet.cpp` | Packet fields, packet types, payload layout, path/transport fields, read/write helpers, validation rules. |
| `rng` | `.reference/meshcore/src/Utils.h`, `.reference/meshcore/src/Utils.cpp` | Upstream RNG API shape and deterministic helper behavior; byte source itself remains platform-provided. |
| `utils` | `.reference/meshcore/src/Utils.h`, `.reference/meshcore/src/Utils.cpp` | Crypto helper semantics, MAC layout, hex helpers, text parsing helpers, padding and boundary behavior. |
| `identity` | `.reference/meshcore/src/Identity.h`, `.reference/meshcore/src/Identity.cpp` | Identity and local identity data shape, key handling, sign/verify behavior. |
| `clock` | `.reference/meshcore/src/Dispatcher.h`, `.reference/meshcore/src/MeshCore.h` | Millisecond and RTC abstraction semantics; time source is supplied by host platform hooks. |
| `radio` | `.reference/meshcore/src/Dispatcher.h`, `.reference/meshcore/src/Dispatcher.cpp` | Radio abstraction expected by dispatcher; hardware operations use host platform hooks. |
| `packet_manager` | `.reference/meshcore/src/Dispatcher.h`, `.reference/meshcore/src/Dispatcher.cpp`, `.reference/meshcore/src/helpers/StaticPoolPacketManager.h`, `.reference/meshcore/src/helpers/StaticPoolPacketManager.cpp` | Support module for packet queueing, allocation, delayed routing, outbound ownership, capacity behavior. |
| `dispatcher` | `.reference/meshcore/src/Dispatcher.h`, `.reference/meshcore/src/Dispatcher.cpp` | Receive, send, delayed send, ACK, timeout, CAD, retry, duty-budget and routing behavior. |
| `mesh` | `.reference/meshcore/src/Mesh.h`, `.reference/meshcore/src/Mesh.cpp` | Mesh packet construction, flood/direct routing, decrypt/dispatch, path handling, callbacks to runtime and host platform hooks. |
| `tables` | `.reference/meshcore/src/Mesh.h`, `.reference/meshcore/src/Mesh.cpp`, `.reference/meshcore/src/helpers/SimpleMeshTables.h` | Support module for peer/channel lookup semantics and minimal table behavior required by protocol logic. |
| `group_channel` | `.reference/meshcore/src/Mesh.h`, `.reference/meshcore/src/Mesh.cpp` | Channel secret/hash data shape and group payload matching semantics. |
| `advert_data` | `.reference/meshcore/src/helpers/AdvertDataHelpers.h`, `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp`, `.reference/meshcore/src/helpers/UTF8Helpers.h` | Support module for builder/parser behavior and UTF-8-safe advert-name truncation. |
| `text_data` | `.reference/meshcore/src/helpers/TxtDataHelpers.h`, `.reference/meshcore/src/helpers/TxtDataHelpers.cpp` | Support/runtime evidence for TXT/GRP wire text behavior used by runtime messages. |

### Layer 1 Boundary

Layer 1 must avoid host policy. It should not know where identities, peers,
channels, telemetry, settings, or contacts are stored. Those are supplied
through runtime and host platform hooks.

Layer 1 should not read from examples. If an example reveals a missing helper
requirement, promote the exact helper behavior into `src/support` explicitly
and cite the upstream source file that defines it.

## Layer 2: Runtime

`src/runtime/` translates upstream runtime behavior into a host-driven C
runtime.

The runtime is not a thread, not a Zephyr service, and not an Arduino loop. It
is a deterministic event pump entered by the host.

Target runtime responsibilities:

- initialize and reset runtime state
- accept public typed requests
- convert requests into protocol operations
- accept raw radio RX frames from the host
- accept TX completion from the host
- accept timer expiry from the host
- maintain pending request correlation
- publish observable results through explicit platform events
- schedule next work through host callbacks or explicit deadline reporting

Runtime source layout follows behavior responsibility:

- `meshcore.c`: singleton context, lifecycle, timer/deadline pump, radio RX/TX
  ingress, and protocol callback registration
- `meshcore_runtime_request.c`: typed public request execution
- `meshcore_runtime_cli.c`: typed CLI framing, delivery and synchronous reply
  routing/timing; hosts own authorization, replay policy and command execution
- `meshcore_runtime_pending.c`: ACK and pending response correlation
- `meshcore_runtime_policy.c`: runtime forwarding and delay policy
- `meshcore_runtime_receive.c`: protocol receive callbacks and receive
  dispatch
- `meshcore_runtime_control.c`: zero-hop control request behavior
- `meshcore_runtime_event_publish.c`: host-facing event construction and
  platform publication helpers

### Runtime State And Memory Ownership

The public runtime interface is a singleton facade. Internally, the runtime
state is held in a `struct meshcore_runtime` context selected through the
private `meshcore_runtime_context_get()` helper. The current implementation
selects the default process-wide context for all public `meshcore_*` entry
points; the public API supports one process-wide instance.

Runtime packet ownership uses fixed arenas instead of heap allocation.
`meshcore_packet_queue_manager_prepare()` binds each queue manager to storage
embedded in the manager object and rejects pool sizes larger than
`MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE`. Full runtime builds may size the
arena with `MESHCORE_RUNTIME_PACKET_POOL_SIZE`; downstream hosts that expose a
local packet-pool setting should pass that value into both macros.

White-box runtime inspection symbols are test-only. Test targets that need
`meshcore_test_runtime_*` hooks compile the runtime sources with
`MESHCORE_ENABLE_TEST_HOOKS`; production consumers of
`MESHCORE_RUNTIME_LIBRARY_SOURCES` do not link those symbols.

Layer 2 behavior evidence comes from these upstream files.

| Runtime behavior area | Upstream evidence | Notes |
| --- | --- | --- |
| Base chat request and receive behavior | `.reference/meshcore/src/helpers/BaseChatMesh.h`, `.reference/meshcore/src/helpers/BaseChatMesh.cpp` | Main behavior source for advert, peer message, group message, request, path, trace, telemetry and callback semantics. |
| Text/group payload behavior | `.reference/meshcore/src/helpers/TxtDataHelpers.h`, `.reference/meshcore/src/helpers/TxtDataHelpers.cpp` | Runtime message payload encoding and decoding behavior. |
| Advert payload behavior | `.reference/meshcore/src/helpers/AdvertDataHelpers.h`, `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp` | Runtime advert app-data encoding and parsing. |
| Contact and channel protocol fields | `.reference/meshcore/src/helpers/ContactInfo.h`, `.reference/meshcore/src/helpers/ChannelDetails.h` | Evidence for the peer identity/path and channel secret/hash fields consumed by runtime behavior. The caller owns any Contact/Channel business abstraction. |
| Companion runtime flow | `.reference/meshcore/examples/companion_radio/main.cpp` | Example-level behavior flow and request usage. |
| Companion mesh subclass behavior | `.reference/meshcore/examples/companion_radio/MyMesh.h`, `.reference/meshcore/examples/companion_radio/MyMesh.cpp` | Concrete behavior hooks and role-specific runtime decisions. |
| Server CLI reply behavior | `.reference/meshcore/examples/simple_repeater/MyMesh.cpp`, `.reference/meshcore/examples/simple_room_server/MyMesh.h`, `.reference/meshcore/examples/simple_room_server/MyMesh.cpp`, `.reference/meshcore/examples/simple_sensor/SensorMesh.cpp` | Legacy/explicit CLI framing and role-specific reply delays; the room header supplies `SERVER_RESPONSE_DELAY`. ACL state, replay checks and command execution remain host-owned. |
| Companion preferences and storage context | `.reference/meshcore/examples/companion_radio/NodePrefs.h`, `.reference/meshcore/examples/companion_radio/DataStore.h`, `.reference/meshcore/examples/companion_radio/DataStore.cpp` | Caller-owned context only; do not import contact/channel abstractions or persistence implementation. |

### Runtime Boundary

Runtime may depend on `src/core`, `src/support`, and the explicit platform
hook abstraction. It must not depend on a concrete host.

Runtime should expose stable public operations for:

- lifecycle
- radio RX injection
- radio TX completion
- timer expiry
- local advert request
- peer advert replay
- direct peer message
- typed CLI data/command send and authenticated receive with bounded host replies
- channel message
- path discovery
- trace path
- telemetry request
- generic binary request only if the upstream behavior is intentionally kept
- anonymous encrypted peer datagrams with explicit flood, known-direct, or
  caller-supplied authenticated return-path policy

Public C function names live in `include/meshcore/runtime.h`. Do not add
public names solely because an upstream class or helper exposes a similar
method.

## Layer 3: Platform Boundary

The canonical host boundary is `meshcore/platform.h`. It defines
the direct singleton `meshcore_platform_*` hook functions and shared host data
shapes required by the full runtime. A platform that links the runtime must
provide those hook symbols directly; unsupported optional features are explicit
host stubs, not weak library fallbacks or runtime-installed function tables.

The runtime routes platform primitives, host policy/data lookup, dispatcher
diagnostics, mesh policy hooks, and event publication through
`src/platform/meshcore_platform_bridge.c`. That bridge calls the canonical
platform hooks declared by `meshcore/platform.h` and must not install or dispatch
through runtime-installed function tables.

The public include directory is intentionally split by platform role:

- `meshcore/platform.h` is the host-implemented API surface.
- `meshcore/runtime.h` is the host-callable runtime API surface.
- `meshcore/types.h` owns shared constants and data shapes.

The current public include policy is:

- `meshcore/runtime.h`, `meshcore/types.h`, and `meshcore/platform.h` are the
  canonical host-facing headers.
- `meshcore/platform.h` must not expose core-private structures. Runtime bridge
  code converts internal packets, identities, and group channels into borrowed
  public view types from `meshcore/types.h` before calling platform hooks.
- Callers include the canonical nested header for the role they implement or
  consume. Reference harness declarations belong in private test support.

Downstream services are explicit host adapters. They implement the
`meshcore_platform_*` hooks outside this repository and keep RTOS messaging,
settings, companion framing, public product APIs, and board policy outside the
generic library. Private adapter helpers may wrap product-specific state, but
they must not become generic MeshCore APIs.

### Boundary Ownership

| Surface | Owner and validation |
| --- | --- |
| Public constants, limits, and data views | `meshcore/types.h`; host adapters validate any mirrored constants with compile-time assertions. |
| Host-callable lifecycle, event ingress, and requests | `meshcore/runtime.h`; runtime API tests verify arguments, results, and observable behavior. |
| Host primitives, policy, lookup, and publication | `meshcore/platform.h`; the platform bridge forwards calls to the host's linked implementations. |
| Primitive wrappers | `src/core/meshcore_clock.c`, `meshcore_rng.c`, and `meshcore_radio.c` route through `meshcore_platform_bridge_*`. |
| Path encoding and unknown-route conversion | The library owns its public field semantics; adapters validate conversions to host-owned data. |
| Source selection | `cmake/meshcore_sources.cmake`; full-runtime consumers use `MESHCORE_RUNTIME_LIBRARY_SOURCES`, while focused tests select smaller manifest groups. |

Host and library request validation may overlap intentionally at their public
boundaries. Adapters use the canonical library limits and distinguish encoded
path fields from byte lengths. Generic source files depend only on the public
platform contract, with host storage, RTOS, UI, and hardware headers kept in
the embedding application.

### `meshcore/runtime.h`

The callable runtime interface lives in `meshcore/runtime.h`. It should contain:

- lifecycle entrypoints
- event injection entrypoints
- typed request entrypoints
- callback or deadline contract needed by host scheduling

It must not expose concrete host, Arduino, storage, Bluetooth, board, or test
types.

### `meshcore/platform.h`

The canonical host implementation contract lives in `meshcore/platform.h`.
Platform hook categories include:

- monotonic time
- RTC time
- random bytes
- radio send and radio state primitives
- airtime/scoring helpers when required by dispatcher compatibility
- cryptographic primitives required by protocol helpers
- telemetry sensor fetch only when the runtime answers upstream telemetry
  requests directly

- local node identity and profile
- peer identity/path lookup and path update
- channel secret/hash lookup
- host encryption hooks if keys or storage are host-owned
- dispatcher policy knobs
- message, ACK, advert, path, trace, telemetry, and error publication

Hook functions should stay small platform primitives, policy lookups, or event
publication bridges. They may adapt a downstream host internally, but those
types must not appear in the generic header. Contact and Channel business
objects remain caller-owned and must not become MeshCore library data
projections.

## Upstream Sync Workflow

The locked upstream reference state is recorded in `upstream.lock`.

When upstream changes, review by layer.

### Step 1: Protocol API Surface

Compare `.reference/meshcore/src` against the Layer 1 table.

For each changed upstream class/helper:

- identify changed functions, constants, fields, and boundary behavior
- decide whether the target C protocol API changes
- decide whether runtime behavior is affected
- decide whether the platform contract needs a new primitive or policy hook

### Step 2: Runtime Behavior Surface

Compare the Layer 2 evidence files against target runtime behavior.

Track observable behavior:

- advert publish and replay
- direct message send
- flood message send
- channel message send
- ACK correlation
- path discovery
- trace request and response
- telemetry request and response
- timeout cleanup
- malformed or unexpected inbound packets

Do not import example UI, transport, board, or persistence implementation into
the runtime.

### Step 3: Boundary Surface

If an upstream behavior needs host data or platform capability, classify it as:

- host primitive provided through a platform hook
- host policy or data lookup provided through a platform hook
- host integration outside this repository
- intentionally unsupported behavior

## Runtime Behavior And Ownership

Generic binary request/response, channel/group binary datagrams, and raw/control
data expose protocol operations. Host applications own the schemas and services
carried by those operations.

| Layer | Upstream evidence | C surface | Expected behavior | Boundary risk | Validation need | Boundary notes |
| --- | --- | --- | --- | --- | --- | --- |
| Layer 2 runtime | `.reference/meshcore/src/helpers/BaseChatMesh.cpp`, `.reference/meshcore/examples/companion_radio/MyMesh.cpp` | `meshcore_node_binary_request*`, `meshcore_platform_event_binary_response` | Binary request sends upstream-compatible `REQ`; matching `RESPONSE` with the same tag publishes opaque response bytes. | Wrong tag/peer handling can break request correlation or clear pending state too early. | Runtime request tests for wrong-tag then matching-tag response; oracle send-side request parity. | Keep response payload opaque. Higher-level RPC schemas belong to the caller. |
| Layer 2 runtime | `.reference/meshcore/src/helpers/BaseChatMesh.cpp`, `.reference/meshcore/examples/companion_radio/MyMesh.cpp` | `meshcore_channel_data_send`, `meshcore_platform_event_channel_data` | `GRP_DATA` encodes `data_type`, `data_len`, and payload; unknown path floods, known path sends direct. | Treating this as text message or channel-store API would mix business models into protocol runtime. | Oracle send-side parity for flood/direct; runtime receive tests for malformed and valid datagrams. | Channel secret/hash is protocol input. Channel business objects remain caller-owned. |
| Host integration | `.reference/meshcore/examples/companion_radio/MyMesh.cpp` | Host command/push adapters outside this repository | Companion low-level command frames map to host request APIs and response push frames. | Frame-shape drift breaks phone/app compatibility even when this library's wire behavior is correct. | Downstream companion protocol tests for commands, errors, and push frames. | This stays outside this repository; it is a host service adapter. |
| Host service | Upstream application behavior only when it uses MeshCore wire features | Message or caller-owned service APIs, not generic MeshCore interface | Message pagination, file chunks, plugin RPC, or app protocols choose binary request or channel datagram as transport. | Putting pagination in this library would make host storage and message models part of the protocol interface. | Service-level tests once a concrete pagination contract is defined. | This library provides opaque transport; the host application owns schemas, storage, retries, and UX. |
| Release hardening | Current C interface and host embedding requirements | Published singleton contract and upstream-aligned releases | Standalone CMake packaging exists. The public runtime remains singleton; a multi-instance API would require an explicit compatibility decision. | A stable interface commitment must account for global-state and host-contract constraints. | Apply the release checks in `docs/versioning.md` to the exact candidate. | Multi-instance support is not promised by the current package. |

## Validation Model

Validation should be designed after the target surfaces are defined.

Expected validation categories:

- Layer 1 protocol parity tests against upstream class/helper behavior
- Layer 2 runtime oracle tests against upstream behavior evidence
- Layer 3 interface and boundary tests for host-visible contracts
- host integration tests outside this repository

Detailed independent test instructions belong in `docs/testing.md`.

## Behavior Classification

Use these labels when planning or reviewing architecture changes:

- `library contract`: behavior owned by a documented library layer
- `upstream evidence`: source file that defines compatibility behavior
- `host boundary`: belongs in the platform contract or host integration
- `excluded`: Arduino/platform/example implementation detail
- `deferred`: a documented capability or validation gap

## Design Invariants

- The generic library remains plain C.
- Layer 1 follows upstream protocol API evidence.
- Layer 2 follows upstream runtime behavior evidence.
- Layer 3 hides platform and host implementation details.
- Scheduling remains host-owned.
- Storage remains host-owned.
- Transport remains host-owned.
- `.reference/meshcore` remains read-only.
- Current code and tests implement the architecture, but upstream compatibility
  evidence remains the arbiter for protocol/runtime behavior.
