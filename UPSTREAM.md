# MeshCore Upstream Evidence

This file records the upstream evidence contract used by this MeshCore C
library. `upstream.lock` records the exact reference revision that current sync
checks should verify.

## Locked Reference

| Field | Value |
| --- | --- |
| Reference path | `.reference/meshcore` |
| Commit | `b599bc511751de3681e8b9e1d7f7a31d5d0dad4b` |
| Commit summary | `Merge pull request #3389 from jbrazio/fix/set-af-validation` |
| Evidence branch | locked `dev` snapshot |
| Nearest companion tag | `companion-v1.17.1` |
| Upstream license | MIT License; retained in [LICENSES/MIT-MeshCore.txt](LICENSES/MIT-MeshCore.txt) |

`.reference/meshcore` is read-only evidence. Do not update it as part of a
normal library change. The `.reference/` directory is intentionally ignored by
Git and is not part of release tarballs. When the reference revision changes,
update this file, `upstream.lock`, API maps, and the sync report output in the
same upstream update.

The library follows the upstream companion tag for version naming. The tracked
`base_tag`, `version_kind`, and `commit` in `upstream.lock` generate the package
metadata. This tree remains a development snapshot based on
`companion-v1.17.1`, identified as `companion-v1.17.1-dev.b599bc511751`.
The tag selects a behavior baseline; wire payload versions remain protocol
fields. See [versioning](docs/versioning.md).

For work requiring locked upstream evidence, prepare the reference checkout in
a fresh clone using the revision from `upstream.lock`:

```sh
mkdir -p .reference
git clone https://github.com/meshcore-dev/MeshCore .reference/meshcore
meshcore_reference_commit="$(sed -n 's/^commit=//p' upstream.lock)"
test -n "${meshcore_reference_commit}"
git -C .reference/meshcore fetch origin "${meshcore_reference_commit}"
git -C .reference/meshcore checkout --detach "${meshcore_reference_commit}"
```

## Evidence Classification

### Protocol Core

These upstream files are the protocol-core baseline. Their public protocol
classes, constants, fields, packet layouts, and boundary behavior should remain
traceable from C implementation and tests:

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

### Promoted Support Helpers

These helper files are not protocol core, but their behavior is intentionally
promoted because current generic library behavior depends on their wire format
or generic data structure semantics:

- `.reference/meshcore/src/helpers/StaticPoolPacketManager.h`
- `.reference/meshcore/src/helpers/StaticPoolPacketManager.cpp`
- `.reference/meshcore/src/helpers/SimpleMeshTables.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp`
- `.reference/meshcore/src/helpers/UTF8Helpers.h`
- `.reference/meshcore/src/helpers/TxtDataHelpers.h`
- `.reference/meshcore/src/helpers/TxtDataHelpers.cpp`

Promoted support helpers should be documented as `support` evidence, not as
top-level protocol core. They may still have Layer 1 parity tests when they
define packet-visible data formats.

### Runtime Evidence

These files define observable runtime behavior for the host-driven C runtime:

- `.reference/meshcore/src/helpers/BaseChatMesh.h`
- `.reference/meshcore/src/helpers/BaseChatMesh.cpp`
- `.reference/meshcore/src/helpers/ContactInfo.h`
- `.reference/meshcore/src/helpers/ChannelDetails.h`
- `.reference/meshcore/examples/companion_radio/main.cpp`
- `.reference/meshcore/examples/companion_radio/MyMesh.h`
- `.reference/meshcore/examples/companion_radio/MyMesh.cpp`
- `.reference/meshcore/examples/companion_radio/NodePrefs.h`
- `.reference/meshcore/examples/companion_radio/DataStore.h`
- `.reference/meshcore/examples/companion_radio/DataStore.cpp`
- `.reference/meshcore/examples/simple_repeater/MyMesh.cpp`
- `.reference/meshcore/examples/simple_room_server/MyMesh.h`
- `.reference/meshcore/examples/simple_room_server/MyMesh.cpp`
- `.reference/meshcore/examples/simple_sensor/SensorMesh.cpp`

The server files supply CLI reply branches and role-specific delay constants
used by the compiled runtime oracle. Example files are runtime and adapter
evidence only. They do not define generic protocol-core architecture.

### Host Adapter Or Excluded Evidence

The following upstream areas may be useful background for a host adapter, but
must not be imported into generic library architecture without an
explicit future promotion:

- board classes and RadioLib wrappers;
- serial, BLE, Wi-Fi, ESP-NOW, RS232, and bridge transports;
- display, button, vibration, UI, and CLI helpers;
- concrete file-system, preference, and identity-store implementations;
- sensor manager implementations except for wire payload helpers that are
  explicitly promoted;
- example application UI and persistence flows.

`src/helpers/RoutingPolicy.h`, anonymous-contact slot
management in `BaseChatMesh`, and application-specific `ANON_REQ` reply-path
parsing belong on the host/runtime-evidence side of this boundary. CAD enablement,
RadioLib collision handling, nRF crypto acceleration, FEM configuration, and
board changes remain platform or excluded evidence.

## Current Behavior Baseline

The library includes behavior from `Utils::isZeroes`,
`AdvertDataParser::isValidName`, and `TXT_TYPE_CLI_COMMAND`. Name validation
is available as a helper; inbound advert acceptance follows the upstream
receive behavior. CHAT client-repeat consumes flood/direct delay policy
fields, with host defaults 0.5/0.2.
CHAT/REPEATER airtime estimation follows the upstream path + payload + 2
formula even for transport-coded packets.
The C host boundary additionally rejects invalid or overflowing delay arithmetic
by returning zero delay; valid upstream policy behavior remains unchanged.

`meshcore_cli_send_to_node` and `meshcore_platform_cli_receive` carry legacy
CLI_DATA and explicit CLI_COMMAND. The runtime owns plaintext framing, unique
timestamps, route selection and reply delay; host callbacks own command
execution, authorization, per-peer replay/retry suppression and publication.
CHAT CLI_DATA is data-only and does not send a flood path-return. Replies use
CLI_DATA without ACK registration. CHAT/REPEATER delay 600 ms, ROOM 300 ms,
and SENSOR 1000 ms, following their respective example evidence. This does
not import legacy plain-text server commands or room post/session services.

Empty-channel filtering belongs to the channel-search host contract. RAW
requests decode multi-byte path fields, and channel data has a 165-byte limit;
the porting guide defines the encoded fields and route boundaries.
Companion frame v14, command 66/reply 29, UI, configuration storage, ACL models,
board commands, hardware and transports remain host or excluded evidence.

The compiled runtime oracle executes selected unchanged BaseChatMesh send and
receive branches, server reply branches and Companion delay methods against
native C scenarios. Clock/RNG, allocation, crypto transport and authorized
command execution are test doubles. It validates plaintext and queued
routing/timing, not upstream ACL implementation, Arduino firmware or RF.

## Sync Rule

When upstream changes, classify each changed file as protocol core, promoted
support, runtime evidence, host adapter evidence, excluded, or deferred before
changing C implementation. The classification should drive source movement,
public API changes, and test placement.

## Sync Check Workflow

Select checks using [the testing guide](docs/testing.md#sync-and-boundary-checks).
Architecture and source-boundary changes use the sync report. Upstream sync,
strict compatibility validation, and releases also require the exact locked
reference checkout; use the strict procedure in that guide.

The sync report already runs the lock check, which verifies the reference
commit and dirty state. A missing reference is a warning for repository-local
boundary checks, but leaves strict upstream validation incomplete. A wrong or
dirty existing reference is a failure, not a reason to reset or update it
without authorization. Ordinary local boundary work can continue without
preparing a reference checkout.

The sync report groups drift into these categories:

- `upstream lock`: reference checkout does not match `upstream.lock`;
- `protocol core`: locked top-level `.reference/meshcore/src` protocol
  evidence files are missing;
- `promoted support`: helper files intentionally imported as generic support
  evidence are missing;
- `runtime evidence`: helper/example files used for host-driven runtime
  behavior are missing;
- `source manifest`: C implementation files under `src` are
  missing from, duplicated in, or stale in the canonical source manifest;
- `generic include boundary`: platform or host-specific names leaked into
  generic `include/` or `src/` C files;
- `public header ownership`: public headers include private implementation
  headers or non-public dependencies;
- `stale source roots`: build/tool files reference unsupported `protocol/`,
  `runtime/`, or `src/port/compat/` implementation roots;
- `test hook boundary`: runtime white-box symbols leaked into public headers
  or lost their `MESHCORE_ENABLE_TEST_HOOKS` guard.
