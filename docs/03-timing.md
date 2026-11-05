<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Speed and drag timing

This is an independent, experimental, zero-rollout policy. It is not an implementation of the official Dragy application or verification rules. Core inputs are raw NAV-PVT horizontal ground speed plus receiver epochs. Latitude/longitude differences do not drive speed or drag distance.

## Seven independent detectors

| Test | Start/finish |
| --- | --- |
| 0–100 km/h | Estimated standing motion onset → 100 km/h |
| 60–160 km/h | Upward crossings of 60 and 160 km/h |
| 100–200 km/h | Upward crossings of 100 and 200 km/h |
| 200–300 km/h | Upward crossings of 200 and 300 km/h |
| Quarter mile | Standing onset → 402.336 m integrated travel |
| Half mile | Standing onset → 804.672 m integrated travel |
| 100–0 km/h | Downward 100 km/h crossing → confirmed estimated zero |

`Engine::arm()` begins a batch, initially disarmed. Each detector produces at most one complete/invalid result per batch. `interrupt()` invalidates active attempts and continuity; completed results remain completed. No PB store, history, selection UI, calendar management or file format is included.

## Quality and continuity

Require a valid 3D solution, at least 6 used satellites, positive horizontal accuracy no worse than 5 m, positive speed accuracy no worse than 0.5 m/s, and decoder speed in 0–150 m/s. Reject speed jumps above 30 m/s². These are this reference's policy choices, not guarantees or vendor rules.

Use unwrapped receiver time for elapsed measurement. Adjacent intervals must be no more than 200 ms and no more than 1.5 times an established smoothed cadence. At a stable 25 Hz, a missing epoch causes an 80 ms gap and invalidates active runs. Duplicate/backward samples do not advance timing or freshness. Clock resets, link/session/loss boundaries, host stalls beyond 500 ms and GPS advancement more than 500 ms ahead of host progression break active timing. Call `poll()` while no data arrives so stale attempts expire.

## Estimators

Speed-threshold crossing between `(t0,v0)` and `(t1,v1)` uses:

```text
t_cross = t0 + (threshold-v0)/(v1-v0) × (t1-t0)
segment_distance = (v0+v1)/2 × (t1-t0)
```

Use seconds and m/s consistently. For a distance crossing inside a segment, let `a=(v1-v0)/dt`, and solve `remaining=v0*t+0.5*a*t²`. The implementation uses `2*remaining/(v0+sqrt(v0²+2*a*remaining))` when acceleration is nonzero and a constant-speed branch otherwise, clamped to the crossing segment. Raw speed is integrated; coordinate-to-coordinate distance and display filtering are not substituted.

A standing launch needs at most 0.3 m/s for one second, a rising 0.5 m/s crossing with slope 0.5–20 m/s², and bounded extrapolation of that slope to zero (at most one second before the previous sample). A candidate remains private until speed is at least max(1.389 m/s, 3 × worst candidate speed uncertainty) for 300 ms in both receiver and host time. It expires after three seconds, cancels on creep/stall/quality/continuity failure, and preserves the original onset and integrated prefix when confirmed. Confirmation is not the start timestamp.

For braking, extrapolate a plausible recent deceleration to zero after speed crosses below 0.3 m/s, then require at least 500 ms stationary confirmation. Bounce cancels that candidate. Report the estimated zero instant, not the end of the confirmation wait. Runs time out at 120 seconds; reversal/stopping rules can abort individual tests.

`DisplaySpeedHold` provides only a presentation zero after two seconds of plausible standstill, with uncertainty-aware exit hysteresis and immediate release at about 5 km/h. Never pass its held speed into `Engine` or edit `Sample` to match a display.

There is no accelerometer trigger, rollout deduction, slope compensation, straight-line enforcement or official result certification. Synthetic analytic profiles exercise the arithmetic and timing policy.
