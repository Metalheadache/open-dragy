<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# 25 Hz request and verification

The isolated request encodes UBX-CFG-VALSET with **CFG-RATE-MEAS = 40 ms**, applied to the **RAM layer only**, over observed `FD01` transport. It does not request persistent flash or battery-backed configuration. The implementation derives and checks the UBX checksum rather than relying on a label in the UI.

Reference: [u-blox M10 interface description](https://content.u-blox.com/sites/default/files/documents/u-blox-M10-SPG-5.20_InterfaceDescription_UBXDOC-304424225-20128.pdf), UBX-CFG-VALSET and CFG-RATE. The output cadence is checked from NAV-PVT receiver timestamps after the request.

The minimal example requests this only when enabled in configuration and the Device Information Model Number (`2A24`) positively identifies `DRG71` with an accepted delimiter. Similar prefixes such as `DRG710` or an unknown/failed model read do not qualify. The operation is retried after a new supported connection/setup, with stale callbacks invalidated by tokens. Optional setup failure leaves the stream usable at its actual cadence.

ATT success means the transport accepted the write; it does not prove that the receiver applied it. `CadenceVerifier` requires 25 consecutive forward 40 ms intervals from receiver iTOW after the request. A different interval/reset invalidates confirmation; duplicate/reordered epochs do not add evidence. BLE callbacks per second and UI refresh rate are not substitutes. The example reports observed cadence and request success separately.


25 Hz gives 40 ms observation spacing. Timing uses the actual receiver intervals and retains gaps rather than filling them with invented GPS samples.
