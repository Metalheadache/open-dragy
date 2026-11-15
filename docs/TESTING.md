<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Tests and replay

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
build/reference_generate build/synthetic.csv
build/reference_replay build/synthetic.csv --arm --expect-complete
build/reference_replay build/synthetic.csv --arm --expect-complete --require-25hz
python3 tools/audit_package.py
```

The C++17 desktop build enables AddressSanitizer and UndefinedBehaviorSanitizer on Clang/GCC, keeps test assertions enabled and treats compiler warnings as errors.

| Suite | Coverage |
| --- | --- |
| Operation results | Exact-token identity publication, cancellation and concurrent payload consistency |
| Complete UBX protocol | Every split, opaque nested PVT, 0/512/oversize payloads, corrupt/truncated auxiliary messages, quality equivalence and signed high-speed boundaries |
| Receiver consumer | Actual example consumer: queued setup boundaries, pending observation, strict 10 Hz rejection, overflow/session recovery and stale/partial replies |
| Control/replies | Matching ACK/NAK, first reply, every partial request boundary, queued/old/lost contexts, sequence wrap and concurrent atomic snapshots |
| Startup/timing | Eight post-setup 40 ms intervals; 80/120/160/200 ms provisional finishes, confirming quality, dense gaps, stale/reset and 10 m bounds |
| Rate controller | 60 profile/opt-in combinations, existing25 skip, optional operation failures/timeouts, reconnect/repair ownership and independent ACK/cadence |
| Decoder | All 101 frame splits; bytewise/batched/noisy/corrupt streams; signed values, bounds, epochs, rollover, reset and CRC |
| Speed filter | Stationary dwell/hysteresis, creep, raw preservation, quality changes, gaps, duplicates and 25 Hz |
| Rate | Unchanged RAM frame/checksum, case-insensitive discovery, strict profile families, legacy exclusion and same-peer hint precedence |
| BLE session | All 65,536 challenge pairs; GATT/CCCD failure paths, setup/readback, recovery and backoff |
| Launch | Prefix preservation at 5/10/25 Hz, rollover, noise/creep, uncertainty and cancellation boundaries |
| Curved profiles | 63 synthetic acceleration/braking/distance results at 5/10/25 Hz, speed noise and arrival jitter |
| Drag | Seven analytic tests, distance/braking variants, interruptions, launch guards and independent results |
| Consistency | Session/fragment isolation, loss/reset/recovery, cadence and 20,000 two-thread FIFO handoffs |
| Geometry | Units, hemispheres, inverse rounding, antimeridian and invalid input rejection |
| Replay | Clean completion, strict-mode deleted epoch, qualified isolated/dense gaps, checksum corruption, unarmed mode, backwards timestamps, oversize and malformed hex |

The clean generator produces 1,125 NAV-PVT frames at 25 Hz, each split into three notification fragments. Replay uses their synthetic arrival timestamps and checks completion of all seven drag detectors.

For parser throughput, build with `-DCMAKE_BUILD_TYPE=Release -DREFERENCE_SANITIZERS=OFF` and run `reference_benchmark`. It reports decoded frame count, elapsed host time, frames per second and buffer sizes.

The package audit verifies the source allowlist, file hashes, relative links, SPDX/copyright map and canonical license texts. Build output, downloaded dependency trees, local configuration and captures are excluded from the repository.

After reviewing source/documentation changes, run `python3 tools/update_manifest.py` to refresh their hashes. If adding or removing a source file, review its scope and license and update `licensing-map.json` first.

## 0.2.0 validation

On 15 November 2026, all **15 suites passed** in both a Debug build with AddressSanitizer/UndefinedBehaviorSanitizer and an optimized Release build with assertions enabled. Separate ThreadSanitizer executions passed the control/reply concurrency and SPSC consistency suites. ESP-IDF 5.4.2 / esp-nimble-cpp 2.3.4 built the ESP32-S3 example in default scan-only/unarmed configuration and with rate requests, provisional profiles and arming enabled. No board was flashed and no receiver/serial device was accessed.

These checks validate software behavior and compilation. Fake GATT/model responses, synthetic timing profiles and the bounded parser tests do not establish real DRG70/Lite compatibility, RF quality, road accuracy or long-soak acceptance. See [update scope](UPDATE_0.2.0.md).

To reproduce the concurrency checks with Clang, build `tests/control_protocol_test.cpp` and `tests/consistency_test.cpp` with `src/telemetry.cpp`, `-Iinclude -std=c++17 -pthread -fsanitize=thread -g`; run each resulting executable. Use a separate build from ASan/UBSan.

### 0.2.1 final review

The expanded 16-suite ASan/UBSan and Release runs, three ThreadSanitizer suites, default/enabled ESP32 builds, privacy/history audit and corrected boundary regressions are recorded in [the final review](REVIEW_0.2.1.md). The operation concurrency check uses `tests/operation_test.cpp` with the same compiler flags as the two threaded checks above.
