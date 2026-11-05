<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Bounded processing and optimization

The methods here protect a 25 Hz stream from application stalls. They are not a board-specific performance tuning package.

- `Parser` uses a fixed 520-byte buffer, explicit frame bounds and no dynamic allocation. It resynchronizes after noise or checksum failure independently of BLE fragment boundaries.
- `Ingress<32,512>` holds at most **31 fragments**, not 31 GPS samples. One slot distinguishes empty from full. Maximum notification payload is 512 bytes; oversize/full conditions are counted and break decoder continuity.
- The ingress path has one serialized notification producer and one consumer. Its indexes are lock-free 32-bit atomics. Do not use it as an MPSC queue or call the producer concurrently from multiple threads. Platform wrappers need to preserve this contract.
- The producer only copies a bounded fragment, timestamps it, and publishes a slot. It does not parse, log to serial, write storage or wait for application locks.
- The consumer owns parser, timing and presentation-filter state. The ESP32 example puts serial output on this worker; a blocked serial sink can still cause queue overflow, which is reported and treated as data loss.
- Any future SD/logging adapter should use its own bounded queue and report its own losses. A capture failure and a timing-ingress failure are distinct. There is no storage driver or background logging implementation in this minimal package.

On the tested host ABI, `sizeof(Parser)` is 544 bytes and `sizeof(Ingress<>)` is 17,176 bytes. Layout can differ by target ABI. Queue sizing must be checked in the actual compiled target. The ESP32 example uses no PSRAM-specific allocation and includes no product task/memory layout.

## Reproduce a benchmark

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DREFERENCE_SANITIZERS=OFF
cmake --build build-release -j
build-release/reference_benchmark
```

The benchmark feeds 250,000 fixed synthetic frames through the actual parser and checks the decode count. It reports elapsed time, throughput and object sizes. Use a Release build without sanitizers when comparing host throughput across changes.

The queue's conservative loss handling is intentional: retaining a plausible-looking time across unobserved bytes would defeat measurement consistency. Prefer visible loss over silent integration across a gap.
