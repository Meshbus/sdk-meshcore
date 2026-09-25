# MeshCore Versioning

Library versions follow the upstream companion tag baseline. The source of
truth is `upstream.lock`: `base_tag` selects the upstream release line,
`version_kind` distinguishes a development snapshot from a release, and
`commit` pins the exact upstream behavior reference. CMake derives package
metadata from these fields without Git or a reference checkout at build time.

## Current Version

| Field | Value |
| --- | --- |
| Upstream baseline tag | `companion-v1.17.1` |
| Version kind | `dev` |
| Upstream evidence commit | `b599bc511751de3681e8b9e1d7f7a31d5d0dad4b` |
| Full version label | `companion-v1.17.1-dev.b599bc511751` |
| Numeric CMake baseline | `1.17.1` |
| Wire payload version | `PAYLOAD_VER_1`, encoded as `0x00` |

This is a development snapshot after the recorded baseline. The locked commit
defines its behavior reference; it is not the upstream `companion-v1.17.1`
release commit.

## Version Labels And Git Tags

- A development snapshot uses `<base_tag>-dev.<12-character-upstream-commit>`.
- A release aligned to an upstream tag uses that exact tag name, for example
  `companion-v1.17.1`. Set `version_kind=release` only after verifying the
  locked commit is the upstream tag's resolved commit and completing the
  release checks below.
- Preserve upstream role prefixes. This library currently follows the
  `companion-v` line; a repeater or room-server firmware tag is not an
  interchangeable baseline.

The full label identifies the upstream baseline and snapshot, not every local
C adaptation revision. Pin this repository's full Git commit in dependency
manifests and record it alongside the full version label in build provenance.
Local fixes on the same baseline keep the label and have distinct Git commits.
Keep published tags immutable; do not reuse a tag for later local fixes.
Create tags only as part of an authorized release operation.

When upgrading upstream, update `upstream.lock` and `UPSTREAM.md` together,
classify affected protocol/support/runtime behavior, update the C port and
integration guidance, and run the relevant evidence checks. A local host-interface
change needs contract tests and a documented host impact under the same baseline.
The package version identifies the upstream baseline; pin the library commit
to select a particular C adaptation.

## CMake Package Compatibility

CMake's numeric version is the `major.minor.patch` part of `base_tag`.
`find_package(meshcore 1.17.1 EXACT CONFIG REQUIRED)` requests that exact
baseline. The package uses `ExactVersion`, so a different numeric baseline is
rejected even without the caller's `EXACT` keyword. Omitting the version
requests no numeric guard.

Numeric matching cannot distinguish a development snapshot from a release or
another C adaptation revision on that baseline. It does not imply compatible
host data layouts or stable public APIs. Installed package configuration also
exports these strings:

| Variable | Meaning |
| --- | --- |
| `meshcore_VERSION_STRING` | Full upstream-based version label. |
| `meshcore_VERSION_KIND` | `dev` or `release`. |
| `meshcore_UPSTREAM_TAG` | Upstream baseline tag, including its role prefix. |
| `meshcore_UPSTREAM_COMMIT` | Full locked upstream commit. |

A host that requires this snapshot can check the metadata after finding the
package:

```cmake
find_package(meshcore 1.17.1 EXACT CONFIG REQUIRED)
if(NOT meshcore_VERSION_KIND STREQUAL "dev" OR
   NOT meshcore_UPSTREAM_COMMIT STREQUAL
       "b599bc511751de3681e8b9e1d7f7a31d5d0dad4b")
  message(FATAL_ERROR "Unexpected MeshCore upstream snapshot")
endif()
target_link_libraries(my_host PRIVATE meshcore::meshcore)
```

A release-only consumer must require `meshcore_VERSION_KIND` to be `release`.
Pin the library Git revision as well when selecting an exact C adaptation.
Rebuild the library and host together with matching public headers and compile
definitions. Review [version selection](porting.md#version-selection)
when upgrading; retain checks for the actual public constants, types and hooks
used by the adapter.

## Wire Compatibility

The packet header's two payload-version bits remain protocol fields. Current
upstream behavior accepts `PAYLOAD_VER_1` (`0x00`) and rejects higher versions.
An upstream tag change does not itself change that encoding. Change supported
wire versions only when the upstream evidence does, and record it in release
notes.

## Pre-Release Checks

Before creating an authorized release tag, verify its upstream commit identity
and complete the native suite, sync/boundary checks, strict upstream validation,
and install/consumer smoke from [the testing guide](testing.md). Reuse results
only for the same candidate and locked evidence. Missing strict upstream
evidence leaves release validation incomplete.

Record the full version label, this library's Git commit, upstream tag and
commit, host integration requirements and validation results in release notes.
Review cross-platform CI, coverage and fuzz/stress results described in
[the test strategy](../PLAN.md#ci-gates). The manual/scheduled upstream workflow
alone does not establish that a release candidate passed these checks.
