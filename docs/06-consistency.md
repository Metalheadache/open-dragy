<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Data consistency and replay

There are three separate questions: was a frame intact, did receiver time progress consistently, and did the host deliver it in time? A checksum answers only the first.

## Rules

| Condition | Handling |
| --- | --- |
| Bad UBX checksum/oversize header | Reject frame, count diagnostics, resynchronize |
| Same receiver iTOW | Duplicate; no elapsed-time or freshness advance |
| Small backwards receiver epoch | Ignore reordered sample; do not establish a new clock |
| GPS week boundary near the week endpoints | Unwrap over 604,800,000 ms |
| Larger backwards epoch/reset or invalid iTOW | Break timing continuity |
| Missing established epoch / excessive interval | Ordinary engine invalidates; strict25 integration may confirm one bounded isolated gap |
| Host stall or compressed backlog | Freshness/latency checks can invalidate timing despite continuous receiver time |
| Disconnect, new subscription or queue loss | Reset partial decoder state and interrupt active attempts |
| Poor fix/accuracy/satellite policy | Do not manufacture an eligible timing result |

`Ingress` versions fragments by subscription session and observed loss revision. A late callback carries its captured old token and is rejected. A loss revision changes on overflow/oversize input; queued fragments from before that loss are discarded conservatively. The consumer resets parser state and invokes a gap hook, so halves of different streams cannot be spliced into a frame. These counters are bounded 32-bit values, not persistent global identities.

The generic decoder's hooks must be provided by the integration. Its gap hook must interrupt timing and reset any presentation filter/cadence state. `Decoder::drain()` and the engine are consumer-owned; they are not thread-safe for concurrent callers. `Engine::poll()` must also be invoked during silence.

UBX's checksum and the supplied CRC32 helper detect accidental corruption; neither authenticates a receiver or a recorded file. The reference does not provide cryptographic trust, storage transactions or product capture formats.

## Portable replay input

CSV has exactly this header and two fields per record:

```text
arrival_us,hex
10000000,b56201075c00
10000000,00000000
```

Each row contains a monotonic host arrival timestamp in microseconds and a nonempty even-length hex string of **at most 512 bytes**, representing one notification fragment. The illustrative bytes above are only a partial packet. Equal arrival times are allowed for batched fragments; backwards host timestamps are rejected. No device identifier or geographic metadata is stored outside the packet itself.

`reference_generate` creates 1,125 synthetic 25 Hz frames over 45 seconds, each split into three fragments, around coordinate (0,0). `reference_replay` feeds those fragments through ingress, decoder and timing in order, using the recorded/synthetic arrival times without sleeping. Detectors remain disarmed unless `--arm` is supplied. `--expect-complete` fails unless all seven complete with no checksum/ingress errors. This is a convenience for the clean synthetic case, not a universal rule for real traces.

Raw UBX bytes alone cannot reconstruct BLE arrival jitter, dropped notification boundaries or host stalls. Do not fabricate regular host timestamps for a real capture and then claim to have validated delivery consistency. The ESP32 example prints decoded samples, not this raw-fragment capture CSV. Producing a real replay trace requires a separate bounded capture adapter that preserves arrival timestamps; no such storage service is included.

Synthetic fixtures are safe to regenerate. Real packets can contain precise locations and receiver timestamps; do not add private recordings, even with device addresses removed. Geometry can reveal a route despite simple coordinate offsets.

## Complete-message dispatch and request boundaries

The 520-byte parser validates complete UBX messages with payloads from 0 to 512 bytes. A valid unsupported message is skipped intact, so a nested NAV-PVT-looking byte string cannot become a GPS sample. Corrupt messages resynchronize bytewise; oversize headers have a separate length-error counter. Only class 01 / ID 07 with exactly 92 payload bytes produces a sample. Borrowed message callbacks must not retain payload pointers.

Each admitted notification also carries a monotonic sequence. Decoder fragment hooks expose stream/loss/sequence, host arrival and parser byte offset before feeding the fragment. `ReceiverDecoder` combines these with coherent gate/reply boundaries. Reply attribution checks the frame's first-byte offset as well as its delivery sequence; a reply queued or partially received before a request is excluded.

Replay retains the original multi-rate mode. Add `--require-25hz` to exercise strict startup admission and confirmed isolated gaps. It needs no actual model/rate request because replay is offline and does not control a receiver. Both modes reject `--expect-complete` on any checksum, length or ingress loss; strict25 does not convert a sustained 10 Hz trace into 25 Hz.

The live `ReceiverDecoder` polls timing before draining a backlog and excludes samples older than 250 ms relative to its poll timestamp from timing admission. Raw/protocol observation still runs. This prevents a paused consumer from publishing a finish from stale queued input. Offline replay deliberately uses recorded arrival timestamps, not wall-clock execution time.
