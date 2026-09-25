# MeshCore Test Plan

Status: test strategy with implemented CI mechanisms and explicitly identified
policy proposals.

Use [docs/testing.md](docs/testing.md) for local commands and task-sized
validation, and [docs/versioning.md](docs/versioning.md) for release acceptance.
The workflow files define enabled jobs and triggers; this document does not
establish remote branch-protection settings. Public headers, test sources, and
inventory tools define the current API and coverage markers.

This plan is for automated CI of the standalone MeshCore C library. Its two
primary purposes are:

1. prove that protocol and observable runtime behavior stay compatible with the
   locked upstream Arduino MeshCore implementation;
2. test the public C API exhaustively enough that API misuse, boundary cases,
   host-hook failures, state-machine mistakes, and package regressions
   are caught before release.

Native CTest validates the library without an RTOS. A separate library-owned
Zephyr suite uses west and Twister for protocol, runtime, and boundary checks.
Both paths use test-local platform hooks. Product services, transport adapters,
storage backends, UI, provisioning, and radio hardware require host-owned tests.

## Source Of Truth

### Layer 1: Protocol Core And Support

Upstream evidence files:

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

Target C surfaces:

- public constants and wire-visible limits in `include/meshcore/types.h`;
- packet, identity, utils, dispatcher, mesh, table, packet-manager, advert,
  text, telemetry, and crypto behavior under `src/core` and `src/support`;
- canonical source groups in `cmake/meshcore_sources.cmake`.

Expected validation style:

- byte-for-byte packet/golden-vector comparison where output is deterministic;
- deterministic boundary tests for all upstream packet/layout limits;
- C implementation tests tied to the evidence file that owns the behavior.

### Layer 2: Runtime

Use the canonical [runtime evidence list](UPSTREAM.md#runtime-evidence) for
chat, companion, and server behavior, plus the
[promoted support helpers](UPSTREAM.md#promoted-support-helpers) for TXT and
advert payload formats. Select the evidence that owns the affected behavior.

Target C surfaces:

- host-callable functions in `include/meshcore/runtime.h`;
- runtime implementation in `src/runtime`;
- host/platform publication and lookup hooks in `include/meshcore/platform.h`;
- fake host/oracle support under `tests/support`.

Expected validation style:

- tests enter through public `meshcore_*` runtime APIs;
- radio RX/TX, timer expiry, TX completion, and host storage are injected by a
  deterministic fake host;
- tests compare observable behavior: serialized radio frames, event callbacks,
  timer deadlines, pending tags, route choices, and host hook calls.

### Layer 3: Public Platform Boundary

Evidence files:

- `include/meshcore/types.h`
- `include/meshcore/runtime.h`
- `include/meshcore/platform.h`
- `src/platform/meshcore_platform_bridge.c`

Target C surfaces:

- every public runtime function;
- every host-implemented platform hook;
- every public struct, enum, constant, payload limit, and event data shape;
- installed CMake package target `meshcore::meshcore`.

Expected validation style:

- compile-only and runtime tests using public headers only;
- interface snapshot and size/limit checks;
- link tests proving all required platform hooks are concrete symbols;
- install/consumer smoke through `find_package(meshcore CONFIG REQUIRED)`.

## Test Architecture

Current test layout:

```text
tests/
  native/              CTest entry points, API/parity tests, inline vectors,
                       and the fuzz smoke harness
  oracle/              compiled upstream runtime harness template
  support/
    fake_platform.*    configurable fake host/platform harness
  zephyr/              ztest/Twister protocol, runtime, and boundary suites;
                       upstream harnesses and test-local platform hooks
```

Keep tests with their existing suites unless a responsibility change warrants
moving them. Use CTest labels to select coverage by purpose:

- `api`
- `parity`
- `runtime`
- `boundary`
- `package`
- `fuzz`

Use `cmake/meshcore_sources.cmake` for all source lists. Full runtime tests use
`MESHCORE_RUNTIME_LIBRARY_SOURCES`. Focused Layer 1 tests can use smaller
manifest groups such as `MESHCORE_PROTOCOL_PACKET_SOURCES` or
`MESHCORE_SUPPORT_ADVERT_DATA_SOURCES`.

## Upstream Parity Strategy

### Parity Level 0: Evidence Lock And Mapping

Purpose: make sure tests are comparing against the intended upstream revision
and that every compatibility-relevant upstream file is classified.

Commands and reference requirements are maintained in the testing guide's
[sync and boundary checks](docs/testing.md#sync-and-boundary-checks).

Rules:

- `meshcore_sync_report.py` is required on every PR.
- `upstream_lock_check.py` is required for releases, upstream-sync work, and
  manual/scheduled strict parity jobs.
- A public PR may run without `.reference/meshcore`; this is allowed only for
  repository-local boundary checks. Release parity requires the exact locked
  reference checkout.

Acceptance:

- for strict upstream validation, all classified evidence files are present in
  the locked reference checkout;
- `upstream.lock`, `UPSTREAM.md`, and `ARCHITECTURE.md` agree;
- every `src/*.c` file is covered by the source manifest;
- no generic `include/` or `src/` file leaks RTOS, host, Arduino, board,
  transport, or UI dependencies.

### Parity Level 1: Golden Vectors

Purpose: compare deterministic byte-level behavior against upstream.

Fixture source:

- generate fixtures from the locked `.reference/meshcore` tree when feasible;
- otherwise hand-author fixtures from the upstream evidence file and cite the
  evidence file in the test name/comment.

Fixture policy:

- native golden vectors are embedded in C tests under `tests/native`;
- the Zephyr Cayenne LPP suite has generated vectors with a
  [separate reference provenance contract](tests/zephyr/protocol/cayenne_lpp_compat/README.md);
- use separate files under `tests/support/fixtures` when generated or shared
  data benefits from them; name those files for the upstream evidence area,
  for example `packet_read_write_v1.json` or `advert_data_vectors.json`;
- fixture regeneration is manual or scheduled, never silent in ordinary PR CI;
- a changed fixture requires explaining whether upstream changed or the C port
  was previously wrong.

Required golden-vector areas:

| Area | Expected parity |
| --- | --- |
| Packet header fields | route bits, payload type bits, payload version bits. |
| Packet path encoding | hash size/count encoding, path byte length, invalid reserved hash size rejection. |
| Packet read/write | serialized bytes round-trip exactly match upstream `Packet::writeTo` / `readFrom`. |
| Packet hash | payload-type and payload hash behavior, including TRACE path-len caveat. |
| Payload type constants | `REQ`, `RESPONSE`, `TXT_MSG`, `ACK`, `ADVERT`, `GRP_TXT`, `GRP_DATA`, `ANON_REQ`, `PATH`, `TRACE`, `MULTIPART`, `CONTROL`, `RAW_CUSTOM`. |
| Identity | public/private key sizes, pub-key hex parsing, hash prefix matching, sign/verify, tamper rejection. |
| Utils | SHA fragments, encrypt/decrypt block behavior, MAC helpers, hex parser, text part parser. |
| Advert data | type/name/location/feature flags, max data length, invalid parser state. |
| TXT helpers | string copy/null padding, blank detection, hex parsing, group data type constants. |
| Tables | duplicate detection and ACK hash semantics. |
| Packet manager | fixed pool allocation, full pool behavior, delayed queue behavior. |
| Mesh packet builders | advert, peer datagram, anonymous datagram, group datagram, ACK, multi-ACK, path return, trace, raw data, control data. |

### Parity Level 2: Runtime Oracle

Purpose: prove that the host-driven C runtime publishes the same observable
behavior as upstream runtime evidence without copying Arduino scheduling.

Oracle input model:

- local node identity and role;
- local advert profile;
- runtime policy;
- peer records with public key, role, `has_out_path`, `out_path_byte_len`,
  `out_path`, last seen SNR, and shared secret;
- channel records with hash and secret;
- monotonic time and RTC;
- deterministic RNG stream;
- incoming raw radio frames;
- requested public runtime operation.

Oracle output model:

- return code;
- serialized outbound radio frames;
- timer arm/cancel calls and deadlines;
- platform events and event payloads;
- peer/channel lookup calls;
- peer seen/path update calls;
- request error callbacks;
- pending request tag and correlation behavior when visible through events.

Required runtime oracle scenarios:

| Scenario | Required parity behavior |
| --- | --- |
| Init/deinit | local identity failure propagates; success initializes radio/runtime; timer arms; deinit resets state. |
| Timer pump | expired deadlines process queued work and schedule the next deadline. |
| RX invalid frame | invalid or truncated radio frames fail without corrupting events or pending state. |
| TX done success/failure | dispatcher queue and pending response state advance according to upstream semantics. |
| Local advert | flood and zero-hop advert requests serialize expected advert packet/app data. |
| Peer advert replay | raw advert length and parser behavior match upstream; event payload preserves identity/app data. |
| Peer message direct | `has_out_path=true` selects a known direct route, including zero-hop when `out_path_byte_len=0`. |
| Peer message unknown | Missing host path lookup or `has_out_path=false` falls back to flood; zero path bytes alone do not mean unknown. |
| Peer message forced flood | `flood=true` overrides known direct path. |
| Peer ACK | expected ACK correlation publishes message ACK and handles duplicate ACKs. |
| Channel message | 16-byte and 32-byte secrets accepted; invalid secret lengths rejected; group text event target prefix matches secret prefix. |
| Channel data | data type, path, payload length, and event publication match group-data evidence. |
| Path discovery | generated/caller tags, path return, and peer path event behavior match companion semantics. |
| Trace path | path requirement, tag/auth fields, SNR arrays, and terminal event state match upstream behavior. |
| Telemetry request | permission-mask inversion and telemetry response bounds match upstream. |
| Binary request/response | tag correlation, flood return path bounds, direct response bounds, and binary response event payloads match upstream. |
| Node discover | role filters, prefix/full-key responses, timestamp filtering, and event payload shape match evidence. |
| Raw data | path and payload length boundaries match public contract and raw packet behavior. |
| Control data | zero-hop control requires bit 7 set in byte 0 and rejects invalid payloads. |

Important contact/path invariant:

- `has_out_path == true` with `out_path_byte_len == 0` is direct zero-hop.
- `has_out_path == false` means the path is unknown and peer sends must fall
  back to flood.
- Companion's encoded wire/storage path length uses
  `MESHCORE_OUT_PATH_UNKNOWN` (`0xff`). The host peer-path interface uses
  `has_out_path` and `out_path_byte_len`; do not put the sentinel into its byte
  count or infer route knowledge from that count alone.

### Parity Level 3: Differential Upstream Harness

Purpose: catch interpretation mistakes where hand-authored fixtures are
insufficient.

Native differential scope:

- Layer 1 packet, advert, TXT, identity, and mesh packet builder behavior.
  Packet, advert, TXT, and inline identity helper behavior are suitable for the
  compiled upstream differential oracle. Mesh packet builders are covered by C
  golden-vector parity tests unless a future harness can compile upstream
  `Mesh.cpp` without importing excluded board, radio, scheduler, storage, or UI
  dependencies.

Approach:

- the implemented upstream oracle harness, `tools/upstream_oracle.py`, compiles
  selected upstream files from `.reference/meshcore` in a scratch directory
  with minimal host stubs, links
  the current C library plus `tests/support/fake_platform.c`, and compares
  packet, advert, TXT, and inline identity helper behavior in-process;
- generated JSON or binary fixtures remain a possible extension; the current
  harness compares behavior in-process.

Policy:

- strict execution is required for release validation and runs in the
  manual/scheduled upstream workflow;
- it is allowed to be skipped in public PRs without `.reference/meshcore`;
- it must not import board, RadioLib, BLE, Wi-Fi, display, button, sensor,
  serial, CLI, or concrete filesystem code.

## Public API Contract Strategy

Public API tests are not optional regression tests. They are the main proof
that this C library is safe for hosts to consume.

### API Surface Inventory

`tools/api_surface_report.py` parses public headers and emits:

- runtime functions from `include/meshcore/runtime.h`;
- platform hook functions from `include/meshcore/platform.h`;
- public structs/enums/constants from `include/meshcore/types.h`;
- coverage markers mapping public runtime functions and types to test files,
  plus the fake platform's hook implementations. CTest labels are maintained
  in `tests/native/CMakeLists.txt`.

The native suite runs this inventory with its required-coverage options.
Markers establish a static mapping, not execution or exhaustive behavioral
coverage. The contract coverage requirements are:

- every public runtime function must be listed in the API test coverage table;
- every public struct/event type must have at least one size/field/limit or
  event serialization test;
- every public constant that maps to upstream protocol or interface behavior
  must be checked in a boundary test.

### Runtime API Test Matrix

Every function in `include/meshcore/runtime.h` needs these categories unless
the function contract makes one category impossible:

- success path;
- call before `meshcore_init`;
- call after `meshcore_deinit`;
- null pointer rejection for every pointer parameter;
- zero length, one byte, maximum length, and maximum-plus-one rejection for
  every buffer parameter;
- invalid enum/flag/type values;
- host hook failure propagation;
- no outbound frame or event on rejected input;
- expected outbound frame/event/timer side effects on accepted input;
- repeated call behavior where state is involved;
- sanitizer-clean execution.

Contract-specific cases supplement the categories above. This table groups
behavior; use the public header and inventory report for the complete current
function list, including delayed and explicit-route variants.

| API family | Contract-specific cases |
| --- | --- |
| Lifecycle | Identity/policy/timer hook failures, repeated init/deinit, deinit after failed init, no stale timer/event state. |
| Timer and radio ingress | Old/new deadlines, next timer scheduling, invalid/valid RX frames, MTU bounds, SNR/RSSI propagation, TX completion without active TX, queue advance and pending timeout interaction. |
| Local adverts and peer advert replay | Flood/zero-hop selection, radio send failure, advert bounds, invalid replay rejection and valid event publication. |
| Peer messages | Forced flood, known direct, unknown path flood, known zero-hop direct, payload bounds and ACK attempt. |
| Channel messages and data | Secret lengths 0/15/16/32/33, payload bounds, group text frame, null/explicit path and encoded length validation, reserved/dev data type. |
| Discovery and trace | Key/path validation, unknown route behavior, null/generated/caller tags, explicit trace path bounds and expected trace frame, discover role filters, prefix/full key and timestamp filtering. The deprecated host-path trace entry returns `-ENOTSUP`. |
| Telemetry | Permission masks 0/all/base/location/environment, tag generation and payload encoding. |
| Binary requests and responses | Payload bounds, generated/caller/duplicate tags, null request, direct response maximum, reduced flood response maximum and return-path bounds. |
| Anonymous datagrams | Payload bounds, flood, known direct and authenticated caller-supplied path behavior; explicit direct variants never fall back to flood, and delayed variants apply their route policy and minimum dispatcher delay. |
| Raw and control data | Path/payload bounds, null path handling, control byte 0 bit 7 validation and zero-hop control frame. |

### Platform Hook Contract Tests

The platform header is implemented by hosts, but this repository must still
test the runtime's expectations of those hooks.

Required contract tests:

- fake platform defines every `meshcore_platform_*` symbol declared by
  `include/meshcore/platform.h`;
- a negative link test or generated symbol inventory detects newly added hooks
  that fake hosts do not implement;
- each hook that returns a value can be configured to return success/failure;
- each event hook records event count and a copy of the last event payload;
- each lookup hook can be configured for hit, miss, invalid data, and failure;
- timer and radio hooks record every call in order;
- crypto/RNG hooks support deterministic vectors and forced failure;
- platform callbacks do not call back into `meshcore_*` APIs.

Required hook groups:

- runtime scheduling hooks;
- time source hooks;
- radio hooks;
- RNG and crypto hooks;
- node identity/config/policy hooks;
- peer and channel lookup hooks;
- dispatcher policy/logging hooks;
- mesh behavior hooks;
- runtime event publication hooks;
- telemetry provider hook.

### Public Type And Contract Tests

Required tests:

- Public constants, type shapes, and bounds are checked deliberately.
- all upstream payload type constants match upstream values.
- all public maximum lengths are tested at `max` and `max + 1`.
- `MESHCORE_OUT_PATH_UNKNOWN == 0xff`.
- `sizeof` and selected `offsetof` checks exist for public event structs.
- event structs preserve copied payload lengths and do not expose library-owned
  private pointers except documented borrowed packet views.
- headers compile as C and C++.
- installed headers do not include private `src/` headers.
- a minimal consumer builds using only `include/meshcore/*.h` and
  `meshcore::meshcore`.

## CI Gates

The implemented jobs and triggers are in
[ci.yml](.github/workflows/ci.yml) and
[upstream-evidence.yml](.github/workflows/upstream-evidence.yml). The first runs
on PRs, pushes to `main`, and manual dispatch; the second runs on manual
dispatch and a weekly schedule. Proposed additional triggers below are not
enabled acceptance automation.

### Required PR Gate

Purpose: run the smallest set that proves parity/API regressions are unlikely.

Logical coverage requirements (the workflow groups these into jobs):

- `documentation`: local Markdown links/shell syntax and warning-clean Doxygen generation.
- `sync-boundary`: `python3 tools/meshcore_sync_report.py --repo-root .`
- `api-contract`: all public runtime API and public type tests.
- `parity-fixtures`: deterministic Layer 1 golden-vector tests.
- `runtime-oracle-smoke`: high-value Layer 2 scenarios: init, invalid RX,
  direct/flood/unknown peer send, channel message, binary response bounds.
- `strict-warnings`: Linux GCC or Clang with
  `-Wall -Wextra -Werror -Wpedantic`.
- `asan-ubsan`: Linux sanitizer run.
- `package-smoke`: install to temporary prefix and build minimal consumer.
- `zephyr`: all declared library ztest scenarios on `native_sim`, with locked
  upstream evidence, API-map checks, fixture licensing, and a result check
  that rejects skipped or missing scenarios and cases.

Local commands are maintained in [docs/testing.md](docs/testing.md). Select
local checks by changed contract; the CI matrix is not a per-edit itinerary.
The [Zephyr test guide](tests/zephyr/TESTING.md) describes its isolated workspace
and distinguishes library-owned tests from host integration coverage.

### Cross-Platform Gate

Purpose: catch compiler, path, and C interface portability issues.

Implemented matrix and deferred portability work:

| OS | Compiler family | Requirement |
| --- | --- | --- |
| Ubuntu 24.04 | GCC | required |
| Ubuntu 24.04 | Clang | required |
| macOS 15 | AppleClang | required |
| Windows 2025 | MSVC | proposed advisory lane; not configured |

Cross-platform jobs should run the same API/parity CTest labels. They are not a
substitute for the API/parity suites.

### Strict Upstream Parity Gate

Purpose: prove compatibility against the locked reference tree.

Enabled automation: manual `workflow_dispatch` and a weekly schedule.
Release-branch, release-tag, and upstream-update triggers remain proposed.
Their absence does not waive strict validation for a release or upstream sync;
run the required checks for the relevant candidate within the authorized task.

Prepare the reference using [UPSTREAM.md](UPSTREAM.md#locked-reference), which
reads the revision from `upstream.lock`. Follow
[strict upstream validation](docs/testing.md#strict-upstream-validation) for
commands. The compiled differential harness exists and is included in the
upstream-evidence workflow.

### Fuzz And Stress Gate

Purpose: find API and parser bugs beyond deterministic fixtures.

Current CI runs a bounded native fuzz smoke test. Extended nightly,
manual stress, and release-branch stress lanes are proposed and are not
configured in the current workflows.

Targets:

- packet read parser;
- advert data parser;
- TXT helper parser;
- Cayenne LPP compatibility parser;
- `meshcore_radio_rx_inject` with random frames;
- public runtime API boundary fuzz for length and null combinations.

Policy:

- fuzzing uses the native fake host;
- crash reproducers become deterministic API or parity regression tests;
- once extended lanes are enabled, their failures block releases until triaged;
  existing fuzz smoke failures already fail their CI job.

### Coverage Gate

Purpose: measure API and parity coverage, not just line coverage.

Metrics:

- public runtime functions covered / total public runtime functions;
- public event structs covered / total public event structs;
- upstream evidence areas with at least one parity test;
- branch/line coverage for parser and runtime request code.

The current report implementation is `tools/coverage_report.py`. It consumes a
GCC/Clang gcov-style coverage build after CTest and prints all `src/` files plus
focused parser/support and runtime request groups.

Policy:

- line/branch coverage is report-only;
- public runtime function coverage markers are required by the implemented API
  inventory test; they do not prove exhaustive behavioral coverage;
- do not use a single global line coverage number as a release criterion.

## Required Checks By Change Type

This table describes changed-contract coverage in CI and release review.
Choose task-sized local execution through [docs/testing.md](docs/testing.md),
reuse valid results, and identify any required evidence still missing.

| Change type | Required checks |
| --- | --- |
| Public runtime API change | API inventory update, API contract tests, type/contract tests, package smoke, cross-platform native. |
| Public platform hook change | fake platform symbol inventory, hook failure tests, link smoke, package smoke. |
| Public type/constant change | host impact notes, size/offset tests, max/max+1 API tests, upstream parity mapping. |
| Layer 1 protocol/support change | golden vectors, relevant Layer 1 parity tests, sanitizer, strict upstream gate if evidence touched. |
| Layer 2 runtime change | runtime oracle tests, API contract tests for touched entry points, sanitizer, strict upstream gate if evidence touched. |
| Layer 3 boundary/package change | public-header tests, install/consumer smoke, cross-platform native. |
| Upstream evidence update | strict upstream lock, fixture regeneration or explicit deferral, changed-layer parity tests. |
| Documentation-only change | Content, references and command consistency; sync-boundary for architecture/evidence/boundary claims, and supporting evidence for changed release claims. |
| Release tag | PR gate, cross-platform gate, strict upstream parity gate, package smoke, coverage report, fuzz/stress review. |

## Pass/Fail Policy

Required PR failures:

- API inventory reports an untested public runtime function.
- Any API contract CTest fails.
- Any parity fixture test fails.
- Any runtime oracle smoke scenario fails.
- Any required Zephyr scenario or case fails, skips, or is absent from results.
- `meshcore_sync_report.py` reports a failure.
- CMake configure/build fails.
- warning-as-error build fails.
- sanitizer reports a finding.
- install/consumer smoke fails.

Allowed PR warnings:

- missing `.reference/meshcore` warning from sync report, only when
  repository-local checks pass;
- coverage percentage below proposed thresholds; line/branch thresholds are not
  currently enforced. Windows has no configured job to produce a result.

Required release failures:

- missing, dirty, or wrong `.reference/meshcore`;
- fixture mismatch without an approved upstream-change explanation;
- public interface change without host impact notes and contract checks;
- any public runtime function without API contract coverage;
- package cannot be consumed through `find_package(meshcore CONFIG REQUIRED)`.
