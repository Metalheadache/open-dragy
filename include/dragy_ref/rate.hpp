// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include "dragy_ref/telemetry.hpp"
namespace dragy_ref::ble {
// u-blox M10 CFG-VALSET: only CFG-RATE-MEAS=40 ms, RAM layer only.
inline std::array<uint8_t,18> pro_rate_frame(){
 std::array<uint8_t,18> b{0xb5,0x62,0x06,0x8a,10,0,0,1,0,0,1,0,0x21,0x30,40,0,0,0};
 for(size_t i=2;i<16;i++){b[16]+=b[i];b[17]+=b[16];}return b;
}
inline bool pro_model(const uint8_t* p,size_t n){return n>=5&&p[0]=='D'&&p[1]=='R'&&p[2]=='G'&&p[3]=='7'&&p[4]=='1'&&(n==5||p[5]==0||p[5]==' '||p[5]=='-');}
// Require 25 consecutive forward 40 ms intervals after the request. ATT
// success or callback frequency alone cannot establish receiver cadence.
class CadenceVerifier {
    Timebase time_;unsigned consecutive_{};
public:
    void reset(){time_.reset();consecutive_=0;}
    bool observe(uint32_t tow){
        auto e=time_.accept(tow);
        if(e==Epoch::Duplicate||e==Epoch::Backward)return confirmed();
        consecutive_=(e==Epoch::Forward&&time_.delta_ms==40)?consecutive_+unsigned(consecutive_<25):0;
        return confirmed();
    }
    bool confirmed()const{return consecutive_>=25;}
};
}
