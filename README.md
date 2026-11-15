<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Open Dragy

Dragy–ESP32 Engineering Reference

[简体中文](README.zh-CN.md)

This standalone reference explains how to receive GPS telemetry from a compatible Dragy over BLE on an ESP32, decode and convert it, verify receiver cadence, and calculate experimental speed and drag measurements. It includes a portable C++17 core, a minimal ESP-IDF serial example, synthetic replay tools, and tests.

## Scope

| Included area | Reference implementation |
| --- | --- |
| Dragy → ESP32 GPS retrieval | Observed FD00/FD02/FD03 handshake, CCCD readback, bounded notification handoff, connection recovery |
| GPS coordinates and conversion | UBX NAV-PVT decoding, signed integer units, local east/north projection and inverse |
| Speed and drag timing | Receiver ground speed, speed/distance threshold interpolation, standing launch and braking-stop estimates |
| 25 Hz operation | Adaptive RAM-only request; measured 40 ms epochs; DRG70/Lite profiles remain provisional |
| Optimization | Fixed-size buffers, short callbacks, separate consumer task, a reproducible parser benchmark |
| Data consistency | Checksums, session isolation, loss boundaries, receiver-time continuity, duplicates, rollover, stale/burst rejection |

The package contains only these methods and the supporting examples, tools, tests, and documentation. It excludes product application code, touchscreen UI, artwork, board/display/touch/power drivers, settings services, persistent PB/history, lap/sector timing, track databases, route management, storage recovery, update tooling, and production firmware images. The ESP32 example is a small development application using serial output; it is not a complete instrument firmware.

## Data flow

- Compatibility is based on observed protocol behavior, not an official Dragy SDK. The default supported rate-request profile is a positively identified `DRG71`. DRG70/DRG70-C and Lite rate requests require a separate **provisional** opt-in; unknown/legacy models retain their stream.
- The bounded parser validates complete UBX messages with payloads up to 512 bytes. Unsupported valid messages remain opaque; only checksummed 100-byte NAV-PVT frames with a 92-byte payload produce GPS samples. A BLE notification may contain part of a frame or multiple frames.
- Coordinates are decoded from the receiver solution. The reference does not calculate a satellite navigation solution. Its local projection is an approximation, not a global geodesic or coordinate-datum conversion.
- Speed uses receiver horizontal ground speed. Elapsed timing uses receiver GPS time-of-week; host monotonic time checks freshness and delivery. Optional stationary display filtering never feeds drag timing.
- Drag timing is experimental, zero rollout, with explicit quality and continuity gates. It does not implement official Dragy validation, slope correction, or road-gradient verification.
- A 25 Hz stream has 40 ms receiver spacing. Threshold interpolation uses adjacent receiver epochs.

This is an independent engineering reference. It is not affiliated with, endorsed by, or an implementation of the proprietary software of Dragy, u-blox, or Espressif. Names identify compatibility and technical sources.

## Desktop build and replay

Requirements: a C++17 compiler, CMake 3.20 or newer, and Python 3.9 or newer for the package audit. No firmware source tree or third-party runtime library is needed for the desktop core.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
build/reference_generate build/synthetic.csv
build/reference_replay build/synthetic.csv --arm --expect-complete
build/reference_replay build/synthetic.csv --arm --expect-complete --require-25hz
build/reference_benchmark
python3 tools/audit_package.py
```

Sanitizers are enabled by default on Clang/GCC. Set `-DREFERENCE_SANITIZERS=OFF` for a separate optimized benchmark build. Tests keep assertions enabled. The generated CSV contains synthetic fragmented packets and synthetic arrival timestamps. Replay consumes recorded timestamps without sleeping; it does not substitute wall-clock execution time for delivery time. See [replay format](docs/06-consistency.md).

## ESP32 example

Requirements: ESP-IDF **5.4.2**, `h2zero/esp-nimble-cpp` **2.3.4**, and an ESP32-S3 target. Dependency sources are fetched by the component manager and are not bundled here.

```sh
# Activate your ESP-IDF 5.4.2 environment first.
cd examples/esp32
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
```

Under **Dragy engineering reference**, leave the peer address empty to scan and list candidates without connecting. Select your receiver's exact BLE address **and address type** before connecting. Do not commit your private configuration. Rate requests, provisional DRG70/Lite requests, and drag arming are separate opt-in settings, disabled by default. The example always requires measured 25 Hz before timing; a 10 Hz stream remains available for diagnostics. Optional identity/rate operations are nonblocking and never overlap after a timeout. See the [retrieval guide](docs/01-retrieval.md) for the connection sequence.

## What changed in 0.2.0

- Complete UBX dispatch prevents nested payload bytes from becoming false GPS samples; protocol and overlapping quality counters help diagnose rejected data.
- Receiver setup observes existing cadence before issuing a RAM-only request, distinguishes ATT completion from matching receiver ACK/NAK, and excludes queued or partial pre-request replies.
- The ESP32 example uses `ReceiverDecoder` and `TimingGate25`: eight fresh post-setup 40 ms intervals qualify timing. An isolated gap up to 200 ms and 10 m is held until the next real 40 ms epoch confirms recovery, including before publishing a finish.
- Discovery is case-insensitive. Provisional DRG70/DRG70-C/Lite profiles preserve model precedence, legacy exclusion and exact address/type selection. The first 128 notifications receive a bounded host-buffer byte audit.

The portable `Engine` keeps its original strict multi-rate behavior; `--require-25hz` selects the new qualification/gap policy in replay. No fresh receiver, driving-accuracy or long-soak acceptance is claimed for this update. DRG70/Lite hardware compatibility remains **provisional**, including their rate-command transport. See [validation and scope](docs/UPDATE_0.2.0.md).

## Engineering notes

1. [Retrieval and minimal ESP32 adapter](docs/01-retrieval.md)
2. [GPS fields and coordinate conversion](docs/02-coordinates.md)
3. [Speed and drag calculations](docs/03-timing.md)
4. [25 Hz request and verification](docs/04-rate.md)
5. [Optimization and benchmarking](docs/05-optimization.md)
6. [Consistency and replay](docs/06-consistency.md)

See [tests](docs/TESTING.md), [attribution](NOTICE) and [licensing](docs/LICENSING.md).

## License

Copyright 2026 Yizhan Wu. Licensed under [MPL-2.0](LICENSE). The two BLE files identified in [NOTICE](NOTICE) retain applicable Apache-2.0 terms alongside MPL-2.0. The [file-level license map](licensing-map.json) records this distinction.
