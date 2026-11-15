<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Receiver profiles, 25 Hz setup and verification

The unchanged UBX-CFG-VALSET request writes **CFG-RATE-MEAS = 40 ms to RAM only** through `FD01`. It does not save flash/battery-backed settings or change constellation/navigation settings. Reference: [u-blox M10 interface description](https://content.u-blox.com/sites/default/files/documents/u-blox-M10-SPG-5.20_InterfaceDescription_UBXDOC-304424225-20128.pdf), CFG-VALSET, CFG-RATE and ACK messages.

| Profile | Recognition and rate-write policy | Evidence |
| --- | --- | --- |
| DRG71 / Pro | Strict DRG71 model family; request opt-in | Prior physical transport baseline; this update has software/build validation only |
| DRG70 / DRG70-C | DRG70, DRG70-C, DRG70C model or same-peer advertisement hint | **Provisional**, separate opt-in; actual protocol/rate response needs hardware acceptance |
| Lite | Lite / Dragy Lite / Dragy-Lite / Dragy_Lite / DragyLite / DRG Lite / DRGLite family | **Provisional**, separate opt-in; no numeric Lite model ID is assumed |
| DRG69 | Recognized legacy model overrides modern advertisement hints | No rate write |
| Unknown | May be discovered by FD00; no supported model/hint | No rate write |

Matching is case-insensitive and checks family delimiters; `DRG710`, `DRG70X` and `LiteX` do not qualify. A recognized actual model takes precedence over an advertisement. A successful completed lookup with an unknown/absent model may use a DRG70/Lite hint from the selected address **and address type** only. A failed/timed-out model operation does not authorize a request. A Pro advertisement alone does not authorize the Pro rate write. Reconnect/repair repeats identification; hints are never taken from another peer. Missing FD01 leaves the stream usable.

`REFERENCE_REQUEST_25HZ` enables requests; `REFERENCE_PROVISIONAL_RECEIVERS` separately allows provisional DRG70/Lite requests. Both default off. Manufacturer rate capability alone does not establish this particular BLE command transport. Unknown protocols remain raw diagnostic bytes; no NMEA/Nordic-UART adapter or invented quality/UTC data is introduced.

## Adaptive setup

`RateSetup` reads model 2A24 asynchronously. An eligible writable profile gets a 750 ms observation window. Eight consecutive clean forward 40 ms receiver-iTOW intervals, with a sample no older than 250 ms, establish an existing 25 Hz stream and skip the write. Otherwise the original RAM request is sent once. No 10 Hz request is sent.

Bluetooth write completion, receiver reply and measured cadence are separate evidence. The optional firmware read (2A26) follows rate setup. Tokens invalidate late callbacks; no second optional ATT procedure starts after a timed-out procedure may still be owned by the Bluetooth stack. The timeout retains live decoding. Reconnect starts a new setup context.

`ReplyObserver` accepts only checksum-valid ACK-ACK 05/01 or ACK-NAK 05/00 with exactly two payload bytes matching 06/8A. It excludes queued notifications, frames starting before the request (including split replies), previous sessions and observed loss boundaries. The first matching reply wins. ACK has no transaction ID: this is a bounded temporal association, not definitive command attribution. A NAK stops the confirmation watch; missing ACK does not prevent independent cadence confirmation. ATT/ACK alone cannot establish 25 Hz.

## Timing admission

The ESP example uses `TimingGate25`, separately from the legacy `CadenceVerifier` utility (which retains its original 25-interval API). Each setup boundary clears timing and requires **eight post-boundary 40 ms intervals** after setup is released. Queued pre-boundary notifications cannot qualify it. A 10 Hz stream remains diagnostic data and never qualifies timing. Freshness is 250 ms; stale timing expires after 500 ms. Receiver fix/satellite/accuracy and host-delivery checks remain necessary after cadence qualification.

Raw parsing and diagnostic observation continue during setup. A single isolated timing gap has a separate confirmation policy described in [timing](03-timing.md). Neither callback frequency nor UI refresh is receiver cadence, and missing samples are never invented.

In 0.2.1, an optional operation first processed at or after its three-second deadline is conservatively timed out even if its callback completed meanwhile. `RateSetup::att_owned()` covers pending, cancelled and timed-out procedures. In-place repair must disconnect when it is true; call `start(..., new_connection=true)` only after an actual new link. The default does not retire prior ATT ownership. Completed identity payloads use the atomic exact-token `OperationResult` mailbox; the connection task never clears callback-owned scratch.
