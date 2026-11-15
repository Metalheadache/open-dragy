<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Final review — 0.2.1

Reviewed and corrected on 15 November 2026.

## Findings fixed

1. A held endpoint could finish an attempt before the confirming sample failed dynamics or host-delivery checks. Both original samples now run through the existing engine while public results are held transactionally. A rejection restores the previous results and interrupts the unfinished attempt. Separate synthetic speed-jump and compressed-delivery regressions reproduce the old failure and pass after correction.
2. An optional ATT completion first processed at/after its deadline could advance setup. Deadlines now take priority over completion status. Tests cover model, rate write and firmware deadlines, including exact boundary and pre-deadline behavior.
3. In-place repair could start required GATT setup while an optional procedure remained owned by the stack. Pending/cancelled/timed-out ATT ownership now requires disconnect before repair; only an explicitly new connection clears that state. The original handshake is unchanged.
4. Identity scratch data could be written by an already-entered callback while cancellation/new setup reset it. Scratch is now callback-task-owned, and completed results cross tasks through a coherent atomic mailbox carrying the exact token. Old-token publication cannot become a new operation's result. A concurrent payload test passes ThreadSanitizer.

## Verification

All **16 suites** passed with ASan/UBSan and in optimized Release with assertions enabled. The operation mailbox, control/reply and SPSC consistency tests separately passed ThreadSanitizer. ESP-IDF 5.4.2 / NimBLE 2.3.4 built both default and all-opt-in ESP32-S3 examples. Compiler stack-usage output was inspected; the added gate result snapshot has a 784-byte entry frame, the decoder-task entry frame is 2,656 bytes, and the existing 8,192/12,288-byte task allocations remain unchanged. Static frames do not establish runtime stack headroom.

The original multi-rate engine suites, all 65,536 handshake challenge pairs, complete UBX parser, receiver consumer, startup/gap, replay and receiver-profile tests remain included. No physical receiver, serial port or development board was accessed. No fresh receiver compatibility or physical timing accuracy is claimed. **DRG70/DRG70-C/Lite support remains provisional and separately opt-in.**

## Source and privacy review

The previously published update's 65 files were compared with GitHub by blob hash and mode. Its three reachable commits / 86 distinct blobs were scanned for private workstation paths, device addresses, credential/private-key patterns and out-of-scope product identifiers; no findings were present. The repository had one branch and no release assets. The correction is separately checked against the explicit source/hash/license allowlist before publication.

Manual source review confirms only standalone retrieval, protocol, coordinate, experimental speed/drag, rate, handoff and consistency methods. No product UI/artwork, board/display/touch/power driver, settings data, persistent PB/history, lap/sector logic, track data, storage-recovery service, updater, signing key, firmware image, private recording or backup is included. All added packet/timing data is synthetic. Runtime example logs may contain the user's coordinates/identity and remain excluded from the source package. Existing MPL and retained Apache attribution are unchanged.
