// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: isolated local projection; added checked input/output.
#include "dragy_ref/geometry.hpp"
#include <cmath>
namespace dragy_ref {
namespace {
constexpr double pi=3.14159265358979323846, radius_m=6378137;
bool valid(GeoE7 p) {
    return p.lat_e7>-850000000 && p.lat_e7<850000000 &&
           p.lon_e7>=-1800000000 && p.lon_e7<=1800000000;
}
double wrap(double lon) {
    double w=std::fmod(lon,360.);
    if(w>180)w-=360;
    if(w< -180)w+=360;
    return w;
}
}
bool project(GeoE7 p,GeoE7 o,XY& out) {
    if(!valid(p)||!valid(o))return false;
    out={wrap((double(p.lon_e7)-o.lon_e7)*1e-7)*pi/180*radius_m*std::cos(o.lat_e7*1e-7*pi/180),
         (double(p.lat_e7)-o.lat_e7)*1e-7*pi/180*radius_m};
    return true;
}
bool geographic(XY p,GeoE7 o,GeoE7& out) {
    if(!valid(o)||!std::isfinite(p.east_m)||!std::isfinite(p.north_m))return false;
    double lat=o.lat_e7*1e-7+p.north_m/radius_m*180/pi;
    double lon=o.lon_e7*1e-7+p.east_m/(radius_m*std::cos(o.lat_e7*1e-7*pi/180))*180/pi;
    if(!std::isfinite(lat)||!std::isfinite(lon)||lat<=-85||lat>=85)return false;
    GeoE7 result{int32_t(std::llround(lat*1e7)),int32_t(std::llround(wrap(lon)*1e7))};
    if(!valid(result))return false;
    out=result;return true;
}
}
