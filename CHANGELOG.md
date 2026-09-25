# Changelog

## Initial Release — companion-v1.17.1-dev.b599bc511751

This entry describes the initial source distribution. It does not assert that
a release tag has been published or that release acceptance is complete.

- Platform-neutral C MeshCore protocol and singleton runtime, with host-owned
  scheduling, storage, transport, radio, and application policy.
- Public platform hooks and typed runtime requests, standalone CMake builds,
  installed-package consumption, and external Zephyr adapter support.
- Native CTest and library-owned Zephyr suites for public contracts, protocol
  behavior, and selected upstream comparisons.
- Upstream baseline `companion-v1.17.1`, locked development snapshot
  `b599bc511751de3681e8b9e1d7f7a31d5d0dad4b`, and numeric CMake version `1.17.1`.
  The wire payload version is `PAYLOAD_VER_1`, encoded as `0x00`.
- Apache-2.0 for FoBE Studio-owned contributions, with upstream MeshCore MIT
  and bundled third-party terms retained in [LICENSING.md](LICENSING.md).

[Porting](docs/porting.md) defines the current host contract;
[versioning](docs/versioning.md) defines version selection and release checks.
Record native, strict upstream, Zephyr, downstream, and physical-device results
separately using [the testing guide](docs/testing.md). Documentation of a test
or CI lane is not evidence that a release candidate passed it.
