# MeshCore C Library Test Architecture

This document describes the MeshCore-owned Zephyr test architecture. Tests are organized by the
same three-layer model used by the MeshCore module architecture document.

1. `protocol/`: parity against upstream protocol class/helper behavior
2. `runtime/`: observable runtime behavior against upstream runtime examples
3. `boundary/`: public API, platform hooks, and scheduling contracts

Library source selection is owned by this repository's
`cmake/meshcore_sources.cmake`. Tests resolve that manifest relative to their
own checkout. If Zephyr discovers another MeshCore module, configuration fails.
Runtime and full-library tests should consume `MESHCORE_RUNTIME_LIBRARY_SOURCES`;
focused protocol tests should consume the smallest manifest groups that cover
the module under test. Do not add new hand-maintained complete MeshCore source
lists to tests.

## Upstream-Based Versions

The shared test setup loads this checkout's
`cmake/meshcore_version.cmake` alongside its source manifest and prints the full
upstream-based version label and locked upstream commit during configuration.
They build the checked-out library sources directly; installed CMake package
version matching does not run in this path.

Use this repository's `upstream.lock` for the upstream baseline identity.
The independent Zephyr CI lane pins Zephyr, Mbed TLS and TF-PSA-Crypto to
the revisions recorded in its workflow. The pinned source-free builder image
provides west 1.5.0, Python 3.12.3 and `host/gnu` with GCC 13.3.0; the host
variant does not use the image's Zephyr SDK 1.0.1 cross toolchains. The native
CTest path does not need any of these Zephyr inputs.

To reproduce the isolated test environment, use the pinned project revisions
and workspace layout in `.github/workflows/ci.yml`: Zephyr and its two crypto
modules are west projects; this MeshCore checkout is linked as `meshcore`
outside the west manifest.
Prepare `.reference/meshcore` at the commit in `upstream.lock` as described in
`UPSTREAM.md`, then run from the isolated west top directory:

```sh
meshcore_root="$PWD/meshcore"
python3 "$meshcore_root/tools/upstream_lock_check.py" --repo-root "$meshcore_root"
python3 "$meshcore_root/tests/zephyr/protocol/tools/check_protocol_api_map.py" --repo-root "$meshcore_root"
python3 "$meshcore_root/tests/zephyr/runtime/tools/check_runtime_api_map.py" --repo-root "$meshcore_root"
python3 "$meshcore_root/tests/zephyr/runtime/oracle/tools/check_test_cases_sync.py" \
  --src-dir "$meshcore_root/tests/zephyr/runtime/oracle/src" \
  --cases "$meshcore_root/tests/zephyr/runtime/oracle/test_cases.md"
west twister -T "$meshcore_root/tests/zephyr" -p native_sim --integration \
  --filter runnable -O twister-meshcore --inline-logs -j 2 -c
python3 "$meshcore_root/tests/zephyr/tools/check_twister_results.py" \
  --tests-root "$meshcore_root/tests/zephyr" \
  --report twister-meshcore/twister.json
```

The result checker requires every declared scenario and case to have executed
and passed. The upstream lock and API map checks require the locked reference;
missing evidence is a failure. `native_sim` requires Linux. `qemu_x86` remains
in each scenario's platform declaration and can be built separately when a
Zephyr bootstrap change warrants that portability check.

The runtime API map records Zephyr send coverage for `meshcore_cli_send_to_node`
as deferred. The native CLI suite validates send behavior and selected upstream
comparisons; that coverage does not establish execution in the Zephyr suite.

## Goals

- Make upstream compatibility reviewable by layer.
- Separate protocol parity from runtime behavior.
- Separate generic library contracts from host integration tests.
- Assign each test to the layer whose behavior it verifies.

## Non-Goals

- Do not test current source-file names as the contract.
- Keep concrete RTOS and application-service behavior in host integration tests.
- Do not use upstream example UI, board, transport, or persistence code as
  generic library oracle evidence.

## Test Tree

The test tree is organized by verification responsibility:

```text
tests/zephyr/
  TESTING.md

  protocol/
    advert_data/
    cayenne_lpp_compat/
    clock/
    dispatcher/
    dispatcher_mesh/
    identity/
    mesh/
    packet/
    packet_manager/
    rng/
    tables/
    utils/

  runtime/
    oracle/
    requests/
    topology/

  boundary/
    public_contract/
    platform_contract/

```

Receive, pending, negative and scheduling behavior lives inside the named
runtime and boundary suites; those coverage groups are not separate folders.

## Layer 1: `protocol/` Tests

`protocol/` tests verify the C protocol API against upstream protocol
class/helper evidence from `.reference/meshcore/src`.

They should be mostly focused, deterministic, module-level tests.

`protocol/cayenne_lpp_compat` uses fixed vectors captured from a pinned
CayenneLpp reference instead of linking that C++ library. Its
[provenance and reproduction guide](protocol/cayenne_lpp_compat/README.md)
records the source revision, hashes, captured cases and explicit adapter
contract expectations. This leaf only needs the MeshCore C module; it does
not require a `.reference/meshcore` checkout or a C++ runtime.

### Oracle Source

Use only upstream protocol sources for Layer 1 oracle behavior:

- `.reference/meshcore/src/Packet.h`
- `.reference/meshcore/src/Packet.cpp`
- `.reference/meshcore/src/Utils.h`
- `.reference/meshcore/src/Utils.cpp`
- `.reference/meshcore/src/Identity.h`
- `.reference/meshcore/src/Identity.cpp`
- `.reference/meshcore/src/Dispatcher.h`
- `.reference/meshcore/src/Dispatcher.cpp`
- `.reference/meshcore/src/Mesh.h`
- `.reference/meshcore/src/Mesh.cpp`
- `.reference/meshcore/src/MeshCore.h`
- `.reference/meshcore/src/helpers/StaticPoolPacketManager.h`
- `.reference/meshcore/src/helpers/StaticPoolPacketManager.cpp`
- `.reference/meshcore/src/helpers/SimpleMeshTables.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp`
- `.reference/meshcore/src/helpers/TxtDataHelpers.h`
- `.reference/meshcore/src/helpers/TxtDataHelpers.cpp`

Layer 1 must not use `examples/` as oracle input.

### Required Coverage Areas

`packet/` should cover:

- packet type and payload layout
- direct, flood, and zero-hop route fields
- path and transport-code fields
- read/write helpers
- boundary length handling
- malformed input rejection

`utils/` should cover:

- encrypt-then-MAC behavior
- MAC layout and verification behavior
- AES block iteration and padding behavior
- hex helpers
- text parsing helpers

`identity/` should cover:

- identity and local identity data shape
- public/private key handling
- sign and verify behavior
- malformed key/signature handling

`dispatcher/`, `packet_manager/`, and `mesh/` should cover:

- delayed packet ownership
- queue capacity and allocation behavior
- receive routing
- send routing
- ACK behavior
- timeout behavior
- CAD and duty-budget decisions when protocol-visible
- path and trace packet handling

`tables/` and `group_channel/` should cover:

- peer lookup behavior
- channel lookup behavior
- hash matching behavior
- minimal table behavior required by protocol logic

`advert_data/` and `text_data/` should cover:

- wire encoding
- parser boundary behavior
- missing or malformed field handling
- upstream-compatible truncation or rejection decisions

### Protocol API Map Check

`protocol/protocol_api_map.json` is the Layer 1 method inventory ledger. Its
upstream method rows are generated from selected upstream protocol classes and
helpers, then each row maps to C protocol symbols or is marked as `excluded` /
`deferred` with a reason.

Regenerate the map template from upstream headers:

```sh
python3 tests/zephyr/protocol/tools/check_protocol_api_map.py \
  --repo-root . \
  --map tests/zephyr/protocol/protocol_api_map.json \
  --emit-upstream-template
```

To refresh the checked-in map in place:

```sh
python3 tests/zephyr/protocol/tools/check_protocol_api_map.py \
  --repo-root . \
  --map tests/zephyr/protocol/protocol_api_map.json \
  --refresh-map
```

New upstream methods are emitted as `unmapped`, which intentionally fails the
normal checker until the row is mapped to C symbols or explicitly excluded /
deferred.

Run the checker from the repository root:

```sh
python3 tests/zephyr/protocol/tools/check_protocol_api_map.py \
  --repo-root . \
  --map tests/zephyr/protocol/protocol_api_map.json
```

Run this host check separately from protocol Twister suites. It exits nonzero
when the protocol map is stale or the required upstream reference is unavailable;
no Zephyr build or QEMU application is needed. Use the reference revision from
this repository's `upstream.lock`, and do not omit evidence checks when that
checkout is missing. The native/CI checks own source-manifest and sync-report
validation. The protocol inventory check confirms that the library's C
symbols and evidence map still match the locked upstream reference.

This check prevents inventory drift: missing upstream methods, stale mapped C
symbols, overload-count changes, and undocumented exclusions. It is not a
semantic parity proof. Behavior still needs focused protocol tests that compare
the upstream C++ implementation and the C implementation under equivalent
inputs.

## Layer 2: `runtime/` Tests

`runtime/` tests verify observable behavior of the host-driven C runtime.

They should treat the runtime as a black box:

- call public runtime APIs
- inject radio RX frames
- inject TX completion
- inject timer expiry
- observe outbound raw packets
- observe host platform publication hooks
- observe pending request lifecycle

Runtime tests must not retest packet internals that belong to `protocol/`.

Runtime white-box hooks such as `meshcore_test_runtime_*` are test-only
symbols. Suites that need them must compile runtime sources with
`MESHCORE_ENABLE_TEST_HOOKS`; generic production consumers must not rely on
those names being linked.

### Oracle Source

Use runtime behavior evidence from:

- `.reference/meshcore/src/helpers/BaseChatMesh.h`
- `.reference/meshcore/src/helpers/BaseChatMesh.cpp`
- `.reference/meshcore/src/helpers/TxtDataHelpers.h`
- `.reference/meshcore/src/helpers/TxtDataHelpers.cpp`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp`
- `.reference/meshcore/src/helpers/ContactInfo.h`
- `.reference/meshcore/src/helpers/ChannelDetails.h`
- `.reference/meshcore/examples/companion_radio/main.cpp`
- `.reference/meshcore/examples/companion_radio/MyMesh.h`
- `.reference/meshcore/examples/companion_radio/MyMesh.cpp`
- `.reference/meshcore/examples/companion_radio/NodePrefs.h`
- `.reference/meshcore/examples/companion_radio/DataStore.h`
- `.reference/meshcore/examples/companion_radio/DataStore.cpp`

Use `ContactInfo` and `ChannelDetails` only for protocol fields consumed by
runtime behavior, such as peer public keys, paths, channel secrets, and hashes.
Caller-owned Contact/Channel business abstractions and persistence must not be
projected into generic runtime tests.

### Required Coverage Areas

`runtime/oracle/` should cover behavior where upstream and C runtime can be run
with equivalent fixtures and compared directly.

Required observable surfaces:

- local advert request
- flood advert request
- peer advert replay
- direct peer message
- flood peer message
- channel message
- path discovery request
- path discovery response
- trace request
- trace response
- telemetry request
- telemetry response
- timeout cleanup
- invalid inbound packet handling

`runtime/requests/` should cover public request shape:

- required parameters
- size limits
- request serialization into protocol operations
- no-local-identity behavior
- missing-peer or missing-channel behavior

`runtime/receive/` should cover inbound behavior:

- advert observe/publish
- peer message publish
- channel message publish
- path result publish
- trace result publish
- telemetry result publish
- no-follow-up cases

`runtime/pending/` should cover:

- request correlation
- wrong-peer response ignore
- wrong-tag response ignore
- duplicate response ignore
- second request overwrite behavior
- timeout clearing

`runtime/negative/` should cover:

- malformed raw frames
- invalid encrypted payload variants
- unsupported payload types
- short payloads
- oversized payloads
- unexpected response types

### Runtime API Map Check

`runtime/runtime_api_map.json` maps each public `meshcore_*` runtime API from
`include/meshcore/runtime.h` to its coverage owner, upstream
behavior evidence, and concrete ZTEST names. It is an API/evidence coverage
ledger, not an upstream behavior oracle.

Rows marked `covered` are runtime behaviors backed by upstream evidence and
must list `upstream_evidence`. Rows marked `target_only` are intentional C
host-facade APIs, such as lifecycle or event-ingress calls, and must explain why
there is no direct Arduino public-method counterpart.

Regenerate the map template from `meshcore/runtime.h`:

```sh
python3 tests/zephyr/runtime/tools/check_runtime_api_map.py \
  --repo-root . \
  --map tests/zephyr/runtime/runtime_api_map.json \
  --emit-template
```

To refresh the checked-in map in place:

```sh
python3 tests/zephyr/runtime/tools/check_runtime_api_map.py \
  --repo-root . \
  --map tests/zephyr/runtime/runtime_api_map.json \
  --refresh-map
```

New public APIs are emitted as `unmapped`, which intentionally fails the normal
checker until the row is assigned a status, coverage owners, upstream evidence
or target-only rationale, and concrete tests.

Run the checker from the repository root:

```sh
python3 tests/zephyr/runtime/tools/check_runtime_api_map.py \
  --repo-root . \
  --map tests/zephyr/runtime/runtime_api_map.json
```

Run this host check separately from runtime Twister suites. It exits nonzero
if a public runtime API is missing coverage classification, references stale
ZTEST names, or points at missing upstream evidence files. Missing reference
files remain a validation dependency failure, not a reason to downgrade the
map's coverage classifications. This check does not build or run a Zephyr
application and does not replace the runtime behavior suites.

## Layer 3: `boundary/` Tests

`boundary/` tests verify the C library contract with hosts. They are not
upstream oracle tests.

### `boundary/public_contract/`

Verify:

- public headers compile as both C and C++;
- CLI message type values and event text capacity match the host contract;
- public key, channel secret and path limits have the expected values;
- the C probe links into the C++ test application.

Runtime lifecycle, invalid arguments, error codes and callback timing are
covered by `runtime/requests/` and the other runtime/boundary suites.

### `boundary/platform_contract/`

Verify:

- singleton runtime initialization uses linked `meshcore_platform_*` hooks
- platform hook symbols are direct link-time functions, not installed function
  tables

### `boundary/scheduling/`

Verify:

- host-driven event pump behavior
- timer/deadline behavior
- TX completion behavior
- no reentrant callback requirement
- no hidden thread or workqueue dependency

## Test Classification

Use these labels when reviewing or moving tests:

- `protocol target`: belongs in Layer 1
- `runtime target`: belongs in Layer 2
- `boundary target`: belongs in Layer 3
- `host integration`: belongs outside generic library tests

## Rules For Adding Tests

Before adding a test:

1. Identify the target layer.
2. Identify upstream evidence, unless it is a boundary test.
3. Identify the public or internal target C surface.
4. State whether the test proves protocol parity, runtime behavior, or boundary
   contract.
5. Avoid asserting implementation details that are not part of the target
   layer.

## Rules For Removing Tests

Before removing a test:

1. Classify the behavior it currently protects.
2. Check whether the behavior is covered by upstream evidence.
3. Decide whether it belongs in `protocol/`, `runtime/`, `boundary/`, or host
   integration.
4. Delete it only if it tests an implementation detail outside the contract or
   has been replaced by a target-layer test.

## Host Integration Boundary

Generic `tests/zephyr` tests should stop at the C library boundary.

RTOS service integration, application events, Bluetooth transport, board
configuration, storage backends, and hardware validation belong in host or
service test areas outside the generic library test architecture.

The generic library may use test doubles for platform hooks, but those doubles must
model the C boundary, not a concrete host implementation.
