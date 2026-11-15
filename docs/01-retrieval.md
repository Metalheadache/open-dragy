<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# GPS retrieval over BLE

The observed protocol comes from [DragyDash ESP32](https://github.com/jremick/dragy-dash-esp32/tree/8d2aed470157f03cc4e14b4061e540f9b9b164c8), with attribution retained in `NOTICE`. This is community-observed interoperability behavior, not an official API contract.

## Sequence

1. Scan for service `FD00` or a case-insensitive Dragy/DRG name; advertisements identify candidates, not authorization to connect.
2. Select a specific address and its public/random address type. The example never connects automatically to an arbitrary candidate.
3. Connect, obtain service `FD00`, and discover its characteristics in handle order. With the pinned NimBLE wrapper, complete discovery preserves descriptor-search boundaries.
4. Require readable/writable `FD03`, notifying `FD02`, and its `2902` CCCD.
5. Read at least two bytes `a,b` from `FD03`; write `[a,b,a XOR b,a AND b]` back to `FD03`.
6. Subscribe to `FD02`; read the CCCD and require the little-endian value `0x0001`.
7. Confirm actual decodable notifications. A successful ATT write and CCCD readback prove setup, not a live GPS stream.

The callback copies bytes and a host monotonic timestamp into a bounded SPSC queue. Parsing and timing run in another task. Each subscription captures a session token; disconnect/reconnect and stream repair advance the session. Old callbacks and queued fragments cannot join a new decoder context.

The optional `FD01` receiver-rate write is described separately. The minimal adapter does not read optional battery/status `FD04`, persist pairing, or access storage.

## Recovery

Backoff is capped at 1/2/5/10 seconds. A first stream gets a 180-second grace, with one setup repair after 30 seconds; this preserves a receiver's opportunity to acquire GPS. A decoded NAV-PVT frame without a position fix is still a live stream and does not require immediate reconnect. Once samples have arrived, notification or decoder silence of 10 seconds triggers reconnect. Timing itself expires much sooner (500 ms) and must not bridge a link recovery.

GATT operations execute in the BLE worker, not in notification callbacks or while holding application locks. Optional model/rate transactions use asynchronous host APIs, a polled state machine with deadlines and tokens to ignore late completions. Required setup uses the pinned wrapper's synchronous calls on its dedicated worker.

## Development example

Leave `REFERENCE_PEER_ADDRESS` empty for scan-only operation. Use `menuconfig` to supply your selected device identity locally, including `REFERENCE_PEER_RANDOM` when needed. Do not commit generated `sdkconfig`, candidate logs, or device identities. Raw coordinates are printed to serial after connection; treat those logs as private.

The example defaults to unarmed timing and no rate change. Optional arming starts one batch of seven detectors after boot; a completed/invalid detector is not automatically rearmed. No on-device controls or result persistence are included.

Flash only a separate development board when you choose to run it:

```sh
idf.py -p YOUR_PORT flash monitor
```

The example uses serial output, leaves NVS intact and includes no board-specific display or power driver.

## Diagnostic and capture integration

Model 2A24 and firmware 2A26 reads log at most 48 bytes as sanitized text and exact hex, with full length/truncation recorded. FD01–FD04 presence/properties are reported without requiring optional FD04 battery reads. No device identity is persisted. Serial GPS presentation is capped at five updates/second; parsing and timing remain at the received rate.

The example waits for the decoder consumer to be ready before starting BLE. The first 128 stream notifications per connection are compared byte-for-byte against the SDK's borrowed host mbuf using 32 bytes of scratch space. Diagnostics count chained buffers, mismatches and copy failures. This audit depends on the pinned NimBLE callback order; borrowed pointers are consumed synchronously and never retained after the characteristic callback.

A real capture adapter must additionally wait until its own writer can drain, distinguish file-open from consumer readiness, bound startup waits, and report pre-ready/overflow/write/close losses separately. Preserve all raw notifications during rate setup, including unsupported/corrupt messages. Record loss markers and final totals; valid record CRCs alone do not prove a complete capture. Pause/disconnect producers and drain admitted records before finalizing a capture. The reference supplies no SD/archive/USB maintenance implementation.
