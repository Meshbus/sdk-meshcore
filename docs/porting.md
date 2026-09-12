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

## Migrating To ABI 29

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
not an unset sentinel. These fields previously affected only repeaters.
Supply finite nonnegative factors; the upstream CLI accepts 0 through 2.
As a C boundary guard, invalid numeric values or delays that would overflow
the five-bucket random range yield zero delay for both CHAT and REPEATER.
Both roles now use the upstream path + payload + 2 airtime estimate, excluding
transport-code bytes; base Mesh behavior retains the full raw frame estimate.
Channel-search hooks must return configured records only; do not return empty
slots and do not reject a configured slot solely for an all-zero secret.

RAW and channel-data `path_len` arguments are wire fields, not byte counts:
width is `(path_len >> 6) + 1`, hops is `path_len & 63`, and the buffer has
width times hops bytes. Width 4 is reserved. Channel data alone accepts
`MESHCORE_OUT_PATH_UNKNOWN` to flood. A known zero-hop route stays direct.

The package version is 0.4.0, so CMake consumers of 0.3.x must update their
requested package version as well as host ABI assertions. Companion serial
frame v14 and its CLI command/reply codes belong to the downstream adapter;
these library interfaces alone do not upgrade that transport protocol.
