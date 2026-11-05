<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Tests and replay

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
build/reference_generate build/synthetic.csv
build/reference_replay build/synthetic.csv --arm --expect-complete
python3 tools/audit_package.py
```

The C++17 desktop build enables AddressSanitizer and UndefinedBehaviorSanitizer on Clang/GCC, keeps test assertions enabled and treats compiler warnings as errors.

| Suite | Coverage |
| --- | --- |
| Decoder | All 101 frame splits; bytewise/batched/noisy/corrupt streams; signed values, bounds, epochs, rollover, reset and CRC |
| Speed filter | Stationary dwell/hysteresis, creep, raw preservation, quality changes, gaps, duplicates and 25 Hz |
| Rate | Expected RAM-only configuration frame/checksum and strict DRG71 model matching |
| BLE session | All 65,536 challenge pairs; GATT/CCCD failure paths, setup/readback, recovery and backoff |
| Launch | Prefix preservation at 5/10/25 Hz, rollover, noise/creep, uncertainty and cancellation boundaries |
| Curved profiles | 63 synthetic acceleration/braking/distance results at 5/10/25 Hz, speed noise and arrival jitter |
| Drag | Seven analytic tests, distance/braking variants, interruptions, launch guards and independent results |
| Consistency | Session/fragment isolation, loss/reset/recovery, cadence and 20,000 two-thread FIFO handoffs |
| Geometry | Units, hemispheres, inverse rounding, antimeridian and invalid input rejection |
| Replay | Clean completion, deleted epoch, checksum corruption, unarmed mode, backwards timestamps, oversize and malformed hex |

The clean generator produces 1,125 NAV-PVT frames at 25 Hz, each split into three notification fragments. Replay uses their synthetic arrival timestamps and checks completion of all seven drag detectors.

For parser throughput, build with `-DCMAKE_BUILD_TYPE=Release -DREFERENCE_SANITIZERS=OFF` and run `reference_benchmark`. It reports decoded frame count, elapsed host time, frames per second and buffer sizes.

The package audit verifies the source allowlist, file hashes, relative links, SPDX/copyright map and canonical license texts. Build output, downloaded dependency trees, local configuration and captures are excluded from the repository.

After reviewing source/documentation changes, run `python3 tools/update_manifest.py` to refresh their hashes. If adding or removing a source file, review its scope and license and update `licensing-map.json` first.
