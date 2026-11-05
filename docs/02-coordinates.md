<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Coordinates and units

`Sample` stores native integer values until a computation needs floating point. Fields are decoded explicitly as little-endian; signed 32-bit fields are recovered without unaligned pointer casts. NAV-PVT offsets and units follow the [u-blox M10 interface description](https://content.u-blox.com/sites/default/files/documents/u-blox-M10-SPG-5.20_InterfaceDescription_UBXDOC-304424225-20128.pdf), section 3.14.10.

| Stored field | Conversion |
| --- | --- |
| `lat_e7`, `lon_e7` | degrees = integer / 10,000,000 |
| `height_mm`, `msl_mm` | metres = integer / 1,000; ellipsoid and mean-sea-level heights are distinct |
| `speed_mm_s` | m/s = integer / 1,000; km/h = integer × 0.0036 |
| `heading_e5` | degrees = integer / 100,000 |
| `hacc_mm`, `vacc_mm`, `sacc_mm_s` | receiver accuracy estimates, not measured error bounds |
| `tow_ms` | GPS week-relative receiver epoch, in milliseconds |

NAV-PVT decimal degrees are not NMEA degrees/minutes. Do not apply an NMEA conversion to these fields. The reference contains no WGS84↔GCJ-02/BD-09 or map-provider conversion. Interpret a receiver's coordinate datum using its actual configuration.

## Local east/north approximation

For origin latitude/longitude `lat0,lon0`, with angles in radians:

```text
east  = wrap(lon - lon0) × R × cos(lat0)
north = (lat - lat0) × R
R     = 6,378,137 metres
```

The inverse divides by the same scales and rounds back to 1e-7-degree integers. Longitude differences/results wrap across the antimeridian. `project` and `geographic` reject invalid/nonfinite input and latitudes outside the open interval (-85°,85°). Failed calls leave output unchanged.

This fixed-origin equirectangular approximation is intended for nearby geometry. Error grows with extent, latitude, and direction; an origin close to the points is essential. The inverse uses the same origin and longitude scale as the forward projection.

Do not infer motion from raw coordinate wander alone. The independent display-speed hold demonstrates uncertainty-aware stationarity, but no full route recorder or motion reconstruction is part of this package.
