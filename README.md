# MeshCore C Library

`meshcore` is a platform-neutral C MeshCore protocol/runtime library. It keeps
LoRa wire behavior traceable to upstream Arduino MeshCore while leaving
scheduling, storage, transport, radio hardware, UI, and board policy to the
host.

## Status And Capabilities

This is an independent C implementation maintained by FoBE Studio, based on
[upstream MeshCore](https://github.com/meshcore-dev/MeshCore) evidence. It is a
library for embedding in a host application, not a complete radio firmware.
The current version is **companion-v1.17.1-dev.b599bc511751**, based on the fixed
upstream `dev` snapshot recorded in [the version metadata](docs/versioning.md#current-version).
The host interface is still evolving; see [versioning](docs/versioning.md)
and [changes](CHANGELOG.md) before upgrading.

| Capability | Library surface | Host responsibility / limit |
| --- | --- | --- |
| Adverts, peer and channel text | Encoding, routes, receive events, message ACK correlation | Identity, records, publication and application retries. |
| Channel binary and anonymous data | Typed send/receive and explicit route variants | Application payload schemas and authenticated return-route policy. |
| Discovery, trace, telemetry, binary requests | Protocol framing and bounded pending correlation | Storage, telemetry source, permissions and service interpretation. |
| Raw/custom and control packets | Bounded wire operations | Application meaning and allowed usage. |
| CLI data/commands | Authenticated framing and role-specific reply routing/timing | Authorization, replay suppression and command execution. |
| CHAT/REPEATER/ROOM/SENSOR roles | Selected role-dependent protocol behavior | Full upstream applications are not included; room sessions/posts, provisioning and companion serial/BLE framing remain host-owned. |
| Runtime | One single-threaded, non-reentrant instance | Scheduling, radio, timers, crypto hooks and hardware. |
| Deprecated host-path trace request | Returns `-ENOTSUP` | Use explicit-route `meshcore_node_trace_request`. |

CI is configured for Ubuntu 24.04 with GCC/Clang and macOS 15 with AppleClang.
Windows/MSVC and embedded hardware are not covered by this repository's CI.
These are configured test lanes, not a guarantee for every compiler version or
host. Strict upstream evidence and downstream acceptance are separate checks.

## Public Boundary

Hosts integrate through the headers under `include/meshcore`:

| Header | Role |
| --- | --- |
| `meshcore/platform.h` | Functions the host must implement for platform hooks, host-owned storage lookup, policy, and event publication. |
| `meshcore/runtime.h` | Functions the host calls to initialize the singleton runtime, inject radio/timer events, and submit typed requests. |
| `meshcore/types.h` | Shared constants and public data/view types. |

Headers under `src/` are private implementation details and are not installed
or consumed by platform code.

## Prerequisites

- Library: CMake 3.20 or newer, a C99 compiler, and a CMake build tool
  (for example Make or Ninja).
- Native tests: also a C++ compiler with C++17 support for the header smoke
  check, and Python 3.10 or newer. The Python tools use the standard library.
- Strict upstream comparison: also Git and the
  [locked reference checkout](UPSTREAM.md#locked-reference).

Zephyr, west and Twister are not required for the standalone library. A
compiler/toolchain for the target is needed when cross-compiling; native tests
and the example commands below run on the build host.

## Quick Start

From the repository root, use a new build directory:

```sh
cmake -S . -B build.quickstart -DMESHCORE_BUILD_TESTS=ON \
  -DMESHCORE_BUILD_EXAMPLES=ON
cmake --build build.quickstart
ctest --test-dir build.quickstart --output-on-failure
./build.quickstart/examples/minimal_host/meshcore_minimal_host
```

Expected example output: `meshcore minimal host initialized`.
For a multi-configuration generator, pass `--config Debug` to the build and
`-C Debug` to CTest, and run the executable under its `Debug` directory.

Without the upstream reference, the two upstream oracle tests are **Skipped**.
Other native tests and package smoke still run. This is not strict upstream
compatibility evidence; use the [testing guide](docs/testing.md) to obtain it.

The [minimal host](examples/minimal_host/README.md) is a link/init smoke example.
Its RNG, crypto, identity, clocks, timer and radio are placeholders. Replace
these using the [porting guide](docs/porting.md) before real integration.

## CMake Consumption

For a source checkout inside an application's source tree:

```cmake
add_subdirectory(third_party/meshcore)
target_link_libraries(app PRIVATE meshcore::meshcore)
```

The application must compile its own platform hook implementation. The
library's public headers are under `include/meshcore`; private `src/` headers
are not a host integration API.

The three options `MESHCORE_BUILD_TESTS`, `MESHCORE_BUILD_EXAMPLES` and
`MESHCORE_INSTALL` default to ON for a top-level build and OFF as a subproject.
Tests enable internal test symbols in the library. Use a separate build with
`MESHCORE_BUILD_TESTS=OFF` for distribution.

### Install And Consume

These commands create a distribution build and an installed-package example
consumer without modifying a system installation:

```sh
meshcore_prefix="$PWD/build.install-prefix"
cmake -S . -B build.install -DCMAKE_BUILD_TYPE=Release \
  -DMESHCORE_BUILD_TESTS=OFF -DMESHCORE_BUILD_EXAMPLES=OFF \
  -DMESHCORE_INSTALL=ON
cmake --build build.install --config Release
cmake --install build.install --config Release --prefix "$meshcore_prefix"
cmake -S examples/minimal_host -B build.consumer \
  -DCMAKE_PREFIX_PATH="$meshcore_prefix" -DMESHCORE_FIND_VERSION=1.17.1
cmake --build build.consumer
./build.consumer/meshcore_minimal_host
```

A real consumer uses:

```cmake
find_package(meshcore 1.17.1 EXACT CONFIG REQUIRED)
target_link_libraries(app PRIVATE meshcore::meshcore)
```

See [package compatibility](docs/versioning.md#cmake-package-compatibility) for
version matching. Installed `LICENSE`, `LICENSING.md` and complete texts in `LICENSES/` are under
the configured CMake documentation directory (normally `share/doc/meshcore`).

## Zephyr Integration

This repository declares external Zephyr CMake/Kconfig integration. Adding it
to a west manifest alone does not provide that integration or platform hooks.
See the [Zephyr integration guide](docs/zephyr.md) for the external adapter
layout and its validation boundaries.

## Testing Model

Native tests use CMake + CTest and small host fakes without Zephyr. The
[MeshCore Zephyr suite](tests/zephyr/TESTING.md) uses ztest/Twister for protocol
parity, runtime oracle and boundary coverage in an isolated west workspace.
It compiles this checkout directly with test-local platform hooks.

Host integration tests remain useful for platform-specific adapters, but they
are not the only way to validate the library:

- native CTest: platform-neutral public contracts, core, support, runtime,
  and fake-host behavior;
- Zephyr ztest/Twister: locked upstream comparisons and library contracts;
- upstream sync tools: source manifest and reference evidence drift checks;
- downstream host tests: adapter and board-facing integration outside this
  repository.

## Source Manifest

`cmake/meshcore_sources.cmake` is the canonical source manifest. Standalone
CMake, Zephyr integration, and tests include the same manifest to avoid private
source-list drift.

## Documentation

| Reader | Start here |
| --- | --- |
| Library user | [Quick start](#quick-start), [installation](#install-and-consume), [changes](CHANGELOG.md), [versioning](docs/versioning.md). |
| Host implementer | [Porting](docs/porting.md), [configuration and limits](docs/configuration.md), [Zephyr adapter](docs/zephyr.md), [API reference generation](docs/api.md). |
| Contributor | [Contributing](CONTRIBUTING.md), [testing commands](docs/testing.md), [test strategy](PLAN.md). |
| Maintainer | [Architecture and ownership](ARCHITECTURE.md), [upstream evidence](UPSTREAM.md). |

Use this repository's issue tracker for reproducible bugs and integration
questions; include the compatibility tuple and a minimal reproducer. See
[contribution guidance](CONTRIBUTING.md) for scope and evidence.

## License

FoBE Studio-owned contributions use [Apache-2.0](LICENSE). Upstream-derived
MeshCore portions retain MIT, and bundled Monocypher retains its own terms.

Upstream MeshCore and bundled third-party copyright and license notices are
collected in [LICENSING.md](LICENSING.md).
