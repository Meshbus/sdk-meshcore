# Minimal Host: Link And Initialization Smoke

This example proves that a host can implement the public hook symbols, link
the library, initialize its singleton runtime, and shut it down. It sends no
real radio packets and does not demonstrate a complete event loop.

**The platform implementation is a test double, not a production backend.**
Its fixed random bytes, synthetic keys, substitute digest/HMAC and identity
AES operation provide no cryptographic security. Its clocks do not advance,
its timer does not fire, and its radio only reports acceptance.

## Build And Run

From the repository root, with the [build prerequisites](../../README.md#prerequisites):

```sh
cmake -S . -B build.minimal-host -DMESHCORE_BUILD_TESTS=OFF \
  -DMESHCORE_BUILD_EXAMPLES=ON
cmake --build build.minimal-host
./build.minimal-host/examples/minimal_host/meshcore_minimal_host
```

Expected output:

```text
meshcore minimal host initialized
```

For an installed-package consumer, use the
[installation procedure](../../README.md#install-and-consume).

## Replace Before Integrating

| Placeholder | Real host responsibility |
| --- | --- |
| Fixed RNG and synthetic identity | Suitable entropy, correctly generated key pairs and persistent identity storage. |
| Substitute SHA-256/HMAC and copying AES | Correct cryptographic primitives matching the public hook contracts. |
| Constant uptime and RTC | Monotonic milliseconds and packet timestamp time in seconds. |
| No-op timer | Replaceable absolute-deadline timer that queues expiry to the runtime owner. |
| Radio acceptance stub | Frame copy, real transmission, airtime estimates and queued TX completion. |
| Empty peer/channel lookups | Configured records, route knowledge and shared-secret lookup. |
| Discarded events and fixed policy | Application event handling and intentional routing policy. |

Use the [porting guide](../../docs/porting.md) for sequencing, lifetimes and
error handling. Keep test doubles out of a real host build.
