# Build Configuration And Resource Limits

## CMake Options

| Option | Top-level default | Subproject default | Effect |
| --- | --- | --- | --- |
| `MESHCORE_BUILD_TESTS` | ON | OFF | Native tests, C++/Python tooling and private runtime test hooks in the library. |
| `MESHCORE_BUILD_EXAMPLES` | ON | OFF | The deterministic minimal-host smoke example. |
| `MESHCORE_INSTALL` | ON | OFF | Static library, public headers, package metadata, LICENSE and LICENSING.md installation. |
| `MESHCORE_BUILD_DOCS` | OFF | OFF | Optional Doxygen API target; requires Doxygen. |

Use a new build with tests OFF for a distribution artifact. The library is
static; an installed consumer supplies its own platform hook symbols.

## Compile Definitions

These existing source-build overrides are not CMake cache options. Passing
`cmake -DMESHCORE_RUNTIME_PACKET_POOL_SIZE=12` alone does not define a C macro.
Use `target_compile_definitions` on the source target, or the host build's
compiler-definition mechanism. Rebuild the library after changing them.

| Macro | Default | Scope and effect |
| --- | --- | --- |
| `MESHCORE_RUNTIME_PACKET_POOL_SIZE` | 10 | Runtime packet pool; positive capacity bounded by the packet manager arena. |
| `MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE` | Runtime pool macro when supplied, otherwise 10 | Embedded arena capacity; keep equal to the runtime pool unless a separately validated use requires otherwise. |
| `MESHCORE_RUNTIME_REQUEST_TIMEOUT_MS` | 10000 | Pending correlation timeout in milliseconds. |
| `MESHCORE_RUNTIME_TXT_ACK_DELAY_MS` | 200 | Text ACK scheduling delay in milliseconds. |
| `MESHCORE_RUNTIME_SERVER_RESPONSE_DELAY_MS` | 300 | Applicable binary/telemetry server response delays in milliseconds; CLI uses separate role-specific upstream delays. |
| `MESHCORE_NODE_KEY_PREFIX_BYTES` | 4 | Changes public data shapes; must agree in the library and every consumer translation unit. |

Example for a source subproject:

```cmake
add_subdirectory(third_party/meshcore)
target_compile_definitions(meshcore PRIVATE
  MESHCORE_RUNTIME_PACKET_POOL_SIZE=12
  MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE=12)
target_link_libraries(app PRIVATE meshcore::meshcore)
```

For an intentional key-prefix override, apply it `PUBLIC` to the `meshcore`
target so source consumers and the exported installed target inherit it.
Do not override it only in a consumer of an already compiled library. The
upstream tag does not encode these custom build settings; distribute the
setting alongside the binary and validate all adapters against it.

These macros describe the current source configuration surface, not a promise
that arbitrary combinations preserve upstream timing or binary compatibility.
Internal constants without override guards are implementation details. Policy
values such as `tx_delay_factor` and `direct_tx_delay_factor` are runtime host
hook data, not CMake options; the porting guide describes their defaults.

## Bounded Capacity

Packet storage is embedded in fixed arenas. The current expected-ACK table
has 8 entries, independently of the packet pool. Pending discovery, trace,
telemetry and binary correlation state also have their own bounds; enlarging
the packet pool does not enlarge every pending operation capacity.

Public payload limits are owned by [types.h](../include/meshcore/types.h):
text 160 bytes, service request/direct response 163, anonymous data 136,
channel data 165, and raw/control data 184. Route overhead can reduce response
capacity; channel text can be truncated by the sender-name prefix. Use the
individual runtime API contract instead of treating these as unrestricted
application payload sizes.

Report `-ENOBUFS` to the host queue and retry only under application scheduling
policy. Pool sizing affects static RAM; crypto and callbacks also use stack.
This repository publishes no target-independent RAM or stack guarantee.
Measure the target map, stack high-water and traffic behavior with the selected
compiler, type layout, macros and host queues. Native tests do not establish
those hardware resource bounds.

See [request results and recovery](porting.md#request-results-and-recovery) for
queued requests, radio completion, ACKs and asynchronous timer-arm errors.
