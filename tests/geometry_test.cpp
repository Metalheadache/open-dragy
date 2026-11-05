// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "dragy_ref/geometry.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
#include <initializer_list>
using namespace dragy_ref;
int main() {
    for(auto o:{GeoE7{0,0},GeoE7{450000000,1799990000},GeoE7{-450000000,-1799990000}}) {
        for(auto p:{XY{0,0},XY{1000,2000},XY{-1000,-2000}}) {
            GeoE7 g;XY back;assert(geographic(p,o,g));assert(project(g,o,back));
            assert(std::abs(back.east_m-p.east_m)<.02&&std::abs(back.north_m-p.north_m)<.02);
        }
    }
    XY p;assert(project({0,10000000},{0,0},p));assert(std::abs(p.east_m-111319.490793)<1e-6);
    assert(project({0,-1799990000},{0,1799990000},p));assert(p.east_m>0&&p.east_m<300);
    p={7,8};assert(!project({900000000,0},{0,0},p));assert(p.east_m==7&&p.north_m==8);
    GeoE7 out{7,8};assert(!geographic({std::numeric_limits<double>::infinity(),0},{0,0},out));assert(out.lat_e7==7&&out.lon_e7==8);
    assert(!geographic({0,1e20},{0,0},out));assert(!geographic({0,0},{850000000,0},out));
    puts("PASS geometry: units, inverse rounding, hemispheres, antimeridian, pole/nonfinite/bounds rejection");
}
