// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#pragma once
#include "dragy_ref/telemetry.hpp"
#include <algorithm>
#include <cmath>

namespace dragy_ref::timing {
// Bounds for confirmed timing interpolation; no missing samples are invented.
constexpr uint32_t gap_ms=200;
constexpr double gap_metres=10;
inline double displacement(const Sample& a,const Sample& b){
 constexpr double radians=3.14159265358979323846/180.;
 const double lat1=a.lat_e7*1e-7*radians,lat2=b.lat_e7*1e-7*radians;
 const double dy=(lat2-lat1)*.5,dx=(double(b.lon_e7)-a.lon_e7)*1e-7*radians*.5;
 const double h=std::sin(dy)*std::sin(dy)+std::cos(lat1)*std::cos(lat2)*std::sin(dx)*std::sin(dx);
 return 12742000.*std::asin(std::sqrt(std::clamp(h,0.,1.)));
}
inline bool short_gap(const Sample& a,const Sample& b,uint32_t ms){
 return ms>0&&ms<=gap_ms&&a.position_valid()&&b.position_valid()&&
  displacement(a,b)<=gap_metres&&
  (double(a.speed_mm_s)+b.speed_mm_s)*.5*ms/1000000.<=gap_metres;
}
}
