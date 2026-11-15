<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Open Dragy 0.2.0 — receiver processing and qualified timing

Updated 15 November 2026. This update stays within the original retrieval, coordinate, speed/drag, 25 Hz, optimization and consistency scope.

## Included changes

- Bounded complete-UBX dispatch, opaque unsupported payloads, matching CFG-VALSET ACK/NAK observation, overlapping quality counters and signed high-speed regressions. NAV-PVT field decoding and acceptance thresholds remain unchanged.
- Adaptive rate setup: observe existing cadence, skip unnecessary writes, retain the identical RAM-only 40 ms command, distinguish transport/reply/cadence, bound identity reads and prevent optional ATT overlap after timeout, including in-place repair.
- Case-insensitive discovery and strict receiver families with model precedence, legacy exclusion and exact selected address/type. DRG70/DRG70-C/Lite rate requests require a separate **provisional** opt-in and default off. No unknown numeric Lite model ID is assigned.
- Optional strict25 timing admission and confirmed isolated gaps. Startup needs eight clean post-boundary 40 ms intervals; provisional gap endpoints/finishes wait for a real 40 ms confirming sample. The 200 ms / 10 m limits, quality checks, stale/link/loss boundaries and dense-gap rejection remain explicit. No GPS epochs are invented.
- A shared portable receiver consumer used by both the ESP example and host tests; notification/control boundaries protect against queued input and stale/partial replies. Consumer readiness precedes BLE startup. A bounded first-128-notification SDK-byte audit diagnoses the handoff.
- English and Simplified Chinese READMEs and engineering guides cover the new APIs and their limits. The original multi-rate `Engine`, geometry, stationary display filter and handshake remain available. Replay selects strict25 with `--require-25hz`.

## Validation and limits

All 15 CTest suites passed with ASan/UBSan and in Release with assertions enabled. The two threaded suites separately passed ThreadSanitizer. The ESP32-S3 example built with the pinned ESP-IDF 5.4.2 / NimBLE 2.3.4 dependencies in both default and all-opt-in configurations. Package checks cover source allowlist, hashes, links, per-file licenses and forbidden private content.

All newly added packet and timing fixtures are synthetic. A nested NAV-PVT inside a valid unsupported UBX payload no longer produces a GPS sample, and corrupt auxiliary messages are now counted. Tests exercise provisional finishes and reject unsafe gaps; they do not measure physical timing accuracy. No hardware flash, new receiver trace, live rate acceptance or endurance test was performed for this reference update. Prior DRG71 transport evidence is separate from validation of this snapshot. **DRG70/Lite support remains provisional**, including model-specific GATT/command behavior.

## Publication boundary

The repository contains the scoped portable methods, serial development example, tools, tests, documentation and licensing only. Product UI/artwork, board/display/touch/power drivers, application settings, persistent PB/history, lap/sector engine, track data, route management, storage recovery, updater/deployment code, firmware images, private captures/backups, device identities, local paths and signing keys are excluded. Capture consumer-readiness/loss/drain lessons are documented without importing a product storage implementation.

MPL-2.0 terms and the two existing BLE files' retained Apache-2.0 attribution are unchanged. The source manifest and license map are refreshed for this reviewed scope.
