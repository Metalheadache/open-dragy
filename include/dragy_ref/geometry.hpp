// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#pragma once
#include <cstdint>
namespace dragy_ref {
struct GeoE7 { int32_t lat_e7{}, lon_e7{}; };
struct XY { double east_m{}, north_m{}; };
// Local equirectangular approximation; not a global geodesic or datum conversion.
// Reject invalid input and latitudes outside (-85,85) degrees. Output is
// unchanged on failure. Use a nearby origin; see docs/02-coordinates.md.
bool project(GeoE7 position, GeoE7 origin, XY& out);
bool geographic(XY position, GeoE7 origin, GeoE7& out);
}
