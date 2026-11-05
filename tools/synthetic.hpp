// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#pragma once
#include "dragy_ref/telemetry.hpp"
#include <algorithm>
#include <cmath>
namespace synthetic {
inline auto frame(uint32_t tow,int32_t speed=10000) {
    std::array<uint8_t,100> b{};b[0]=0xb5;b[1]=0x62;b[2]=1;b[3]=7;b[4]=92;
    auto put=[&](size_t at,uint32_t x){for(unsigned j=0;j<4;++j)b[6+at+j]=uint8_t(x>>(8*j));};
    put(0,tow);b[26]=3;b[27]=1;b[29]=16;put(40,600);put(60,uint32_t(speed));put(68,80);
    // Positions deliberately centred on (0,0), unrelated to any recording.
    uint8_t a=0,c=0;for(size_t i=2;i<98;++i){a+=b[i];c+=a;}b[98]=a;b[99]=c;return b;
}
inline double speed(uint32_t ms) {
    constexpr double acceleration=3.7,braking=7.3,peak=92.;
    double t=ms/1000.-2,up=peak/acceleration;
    return t<=0?0:t<up?acceleration*t:std::max(0.,peak-braking*(t-up));
}
}
