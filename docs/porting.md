# MeshCore Porting Guide

`include/meshcore` is the complete platform integration contract. A host should
not include headers from `src/`.

## Header Roles

| Header | Role |
| --- | --- |
| `meshcore/platform.h` | Functions the host must implement. |
| `meshcore/runtime.h` | Functions the host may call. |
| `meshcore/types.h` | Shared constants, limits, and public data shapes. |

## Host Responsibilities

- Provide exactly one link-time implementation of every
  `meshcore_platform_*` hook declared by `meshcore/platform.h`.
- Implement unsupported optional behavior as explicit stubs that return a
  negative errno value, `false`, `0`, or no-op according to the hook category.
- Own scheduling, storage, transport, radio hardware, UI, board policy, peer
  records, channel records, and telemetry sources outside the library.
- Treat packet, identity, and channel view pointers as borrowed for the hook
  call only.

## Runtime Use

The runtime is a single process-wide instance. Host code calls
`meshcore_init()`, injects radio/timer events, submits typed requests, and then
calls `meshcore_deinit()` during shutdown.

Callbacks run synchronously from `meshcore_*` entry points. Callback handlers
must not call back into `meshcore_*`; enqueue follow-up work in the host if
needed.

## Hook Implementation Checklist

All declared hooks need link-time definitions, including unsupported features.
Use the public header for individual signatures; this table explains the
minimum behavior and where a stub is appropriate.

| Hook family | When used | Host implementation / unsupported behavior |
| --- | --- | --- |
| `timer_arm`, `timer_cancel` | Scheduling and cancellation | Replace a single absolute-deadline timer; queue expiry. A successful no-op arm is only suitable for the smoke example. |
| `millis_get`, `rtc_get_current_time` | Scheduling and packet timestamps | Monotonic milliseconds and wall-clock seconds, respectively; use one uptime domain for all event injections. |
| `radio_*` | Initialization, TX and dispatcher decisions | Real state, airtime and frame acceptance. Send returns nonzero for acceptance, **0** for rejection; never negative errno. Optional calibration/reset may be no-ops when unsupported. |
| `rng_random`, `crypto_*` | Identity/protocol cryptography | Real entropy and matching primitives. Crypto failure returns false; deterministic substitutes are only test doubles. |
| `node_*` | Initialization, requests and policy lookup | Fill complete identity, policy and profile outputs; return 0 or a negative errno. Required initialization failures cannot be stubbed as successful empty records. |
| `peer_path_get_by_key`, `peer_next_shared_secret_by_hash` | Send routing and receive secret lookup | Return configured records; `-ENOENT` means absent/end of search. Distinguish unknown route from a known zero-hop route. |
| `peer_seen_update` | Peer observation | Update host state, or return 0 when the host intentionally discards the observation. |
| `channel_*` | Channel send/receive | Compute hashes and search configured records. Search returns 0 for no matches; never return empty storage slots. See each hook for its count/status return convention. |
| `cli_receive` | Authenticated CLI delivery | Enforce authorization and replay policy before execution. Return `-ENOTSUP` if unsupported; return bounded reply bytes, zero, or a documented negative refusal otherwise. |
| `dispatcher_log_*` | Diagnostics | Optional no-op; copy any data retained after return. |
| Dispatcher/mesh policy getters | TX timing and forwarding | Choose deliberate values in the documented units; zero is not a universal default. Boolean forwarding/filter decisions have different meanings. |
| `mesh_on_*`, `event_*`, `runtime_request_error` | Receive and runtime publication | Copy borrowed data before enqueueing. Void observations may be no-ops; discarded `event_*` notifications can return 0. Preserve boolean policy decisions where a hook has one. |
| `telemetry_node_get` | Inbound telemetry requests | Fill bounded output and enforce permission policy; return a negative errno when unavailable. |

## Event Pump And Lifetimes

Serialize **all** runtime calls on one host execution context. The library
provides no thread synchronization. Interrupts, radio drivers and other threads
should copy their input into host-owned events and enqueue those events.
Callbacks also run on the runtime owner's call stack; even an immediately
completed radio operation must queue TX completion instead of reentering.

The following pseudocode uses application-defined queue and driver operations;
it is a sequencing guide, not another compilable backend:

```text
prepare identity, peer/channel records, crypto, radio and host event queue
on the single runtime owner:
    check meshcore_init()
    while running:
        event = wait_for_host_event()
        now = current monotonic uptime, reduced to uint32_t milliseconds
        TIMER:   check meshcore_timer_fired(now)
        RX:      check meshcore_radio_rx_inject(copied bytes, length, RSSI, SNR, now)
        TX_DONE: check meshcore_radio_tx_done(now, success)
        SEND:    check meshcore_message_send_to_node(key, flood, attempt, bytes, length)
        STOP:    stop admitting new work and leave the loop
    stop/quiesce radio and timer delivery, discard stale queued ingress
    meshcore_deinit()
```

`timer_arm(deadline_ms)` takes an **absolute** 32-bit uptime deadline, not a
relative delay. Replace the previous deadline; enqueue an already-due expiry
rather than invoking the runtime inside the hook. Use wrap-aware comparisons
in the host timer adapter and the same clock domain for `millis_get` and
`now_ms`. Packet RTC timestamps are a separate seconds-based clock.

The radio TX buffer is borrowed only until `radio_packet_send` returns.
An asynchronous/DMA driver must copy it before returning success. Route only
the active transmission's completion back to the event pump; the public TX
completion call carries no transaction identifier. Quiesce old driver work
before deinitialization/reinitialization so stale completions cannot reach a
new runtime instance.

All callback inputs, including event structs and nested pointers, are borrowed
for the call unless the header explicitly states otherwise. Copy the needed
bytes into host-owned queue storage; copying a struct containing pointers alone
is insufficient. Output arguments belong to the caller and must be filled
within the documented capacity before return.

## Paths And Lengths

| Surface | Meaning |
| --- | --- |
| `peer_path.out_path_byte_len` | Actual byte count; `has_out_path` says whether the route is known. |
| RAW/channel-data send `path_len`, peer-path receive hook `path_len` | Encoded wire field: width = `(value >> 6) + 1`, hops = `value & 63`, bytes = width times hops. Width 4 is reserved. |
| Explicit anonymous route `path_byte_len` | Actual byte count accompanied by an explicit hash width. |
| TRACE request `path_len` | Actual byte count accompanied by `path_hash_size`; 3-byte hashes are projected to the upstream 2-byte TRACE format. |

For example, wire `0x43` means width 2, three hops and six path bytes. Never
use that encoded value directly as a buffer byte count. A known zero-hop
route remains direct. An unknown peer route uses `has_out_path=false`;
channel-data send accepts `MESHCORE_OUT_PATH_UNKNOWN` to request flood, while
RAW rejects that sentinel.

## Request Results And Recovery

| Result | Meaning and action |
| --- | --- |
| Request returns 0 | The outbound queue owns the packet; this does not mean radio completion or a peer ACK. |
| TX completion succeeds | The radio completed the active frame; peer receipt is still unconfirmed. |
| Message ACK event | The protocol matched a peer ACK; application retry policy remains host-owned. |
| `-ENODEV` | Initialize the runtime before submitting work. |
| `-EALREADY` from init | The singleton is already initialized. |
| `-EINVAL` | Correct the argument, path encoding or length before retrying. |
| `-ENOBUFS` | Bounded packet, outbound or expected-ACK capacity is exhausted; apply backpressure rather than a tight retry loop. |
| `-ENOMEM` from init | Check the compiled packet arena configuration. |
| `-ENOTSUP` | The operation is unsupported, including the deprecated host-path trace API. |
| Other negative host result | Handle the failure according to the originating hook's contract. |

A timer-arm failure during init fails initialization. After a packet has been
queued, a timer-arm failure is published through `runtime_request_error` while
the original request still returns 0; blindly resubmitting could duplicate it.
See [configuration and capacity](configuration.md) for bounds, and
[host security responsibilities](#host-security-responsibilities)
for crypto, identity and command policy.

## Host Security Responsibilities

Provide cryptographically suitable entropy and correct SHA-256, HMAC and AES
hook implementations. Provision correctly generated identity keys and protect
private keys and shared secrets in storage, diagnostics and transport adapters.
The [minimal-host example](../examples/minimal_host/README.md) and native fake
platform use deterministic test doubles, not real security backends.

Authorize CLI commands against the full sender identity and host policy, and
enforce per-peer replay/retry rules before execution. Authentication alone does
not grant command permission. Hosts without CLI return `-ENOTSUP` from the
required receive hook. Apply telemetry/service permission policy, authenticate
caller-supplied return routes, bound host queues and treat received payloads as
untrusted. Follow the buffer lifetime and serialization rules above.

Native tests and selected upstream comparisons cover specific library
contracts. Host authorization, key management and deployed-device behavior
require their own validation.

## Version Selection

Pin the library Git commit, use the upstream-based metadata described in
[versioning](versioning.md), and retain compile-time checks for the actual
constants and data shapes the host uses. Rebuild the host with the matching
library headers and configuration. The numeric CMake version is `1.17.1`;
the full version label identifies the locked development snapshot.

## Current Host Contract

Implement `meshcore_platform_cli_receive()`. Hosts without CLI return
`-ENOTSUP`; there is no weak fallback. CLI_DATA on CHAT nodes has no reply
buffer and must be treated as data. On other roles, legacy CLI_DATA and
explicit CLI_COMMAND can execute only after host authorization and replay
checks. Do not treat `peer.flags` as a generic permission bitmap: resolve the
full sender key against the host's own policy. For servers, suppress stale
timestamps and retries according to the upstream role's policy before
executing a command. The generic library does not persist client ACL state.

The event is borrowed and contains bounded, NUL-terminated input. Return the
number of non-NUL reply bytes written, at most `reply_capacity`; the buffer
also has space for an optional terminator. Return zero or a negative errno
for no reply. The runtime queues the reply after callback return; do not call
`meshcore_cli_send_to_node()` from inside the callback. Offloaded commands may
later use that send interface from the host event pump, but delayed asynchronous
execution is host behavior and does not inherit synchronous reply timing.

Initialize CHAT client-repeat delay factors explicitly: 0.5 flood and 0.2
direct preserve upstream defaults. A factor of zero is a real configuration,
not an unset sentinel.
Supply finite nonnegative factors; the upstream CLI accepts 0 through 2.
As a C boundary guard, invalid numeric values or delays that would overflow
the five-bucket random range yield zero delay for both CHAT and REPEATER.
Both roles use the upstream path + payload + 2 airtime estimate, excluding
transport-code bytes; base Mesh behavior retains the full raw frame estimate.
Channel-search hooks must return configured records only; do not return empty
slots and do not reject a configured slot solely for an all-zero secret.

RAW and channel-data `path_len` arguments are wire fields, not byte counts:
width is `(path_len >> 6) + 1`, hops is `path_len & 63`, and the buffer has
width times hops bytes. Width 4 is reserved. Channel data alone accepts
`MESHCORE_OUT_PATH_UNKNOWN` to flood. A known zero-hop route stays direct.

Companion serial frame v14 and its CLI command/reply codes belong to the host
transport adapter. The adapter implements that framing around the library's
protocol operations.
