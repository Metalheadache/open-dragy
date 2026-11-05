// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/speed_filter.hpp"
#include <algorithm>
namespace dragy_ref {
static bool stationary_quality(const Sample& s) {
    return s.position_valid()&&s.fix==3&&s.satellites>=4&&
        s.hacc_mm<=15000&&s.sacc_mm_s>0&&s.sacc_mm_s<=1000;
}
void DisplaySpeedHold::accept(const Sample& s,uint64_t arrival) {
    // A new sample after a host stall needs a new dwell, even if receiver time
    // appears continuous. Clear the old epoch baseline across a host restart.
    if(seen_&&(arrival<arrival_us_||arrival-arrival_us_>500000)) {
        clear_hold();time_.reset();
    }
    auto epoch=time_.accept(s.tow_ms);
    if(epoch==Epoch::Invalid) { clear_hold();time_.reset();return; }
    // Repeated/out-of-order epochs cannot establish a stationary dwell or keep
    // an old zero alive by refreshing its arrival timestamp.
    if(epoch==Epoch::Duplicate||epoch==Epoch::Backward)return;
    if(epoch==Epoch::Reset||time_.delta_ms>500)clear_hold();
    arrival_us_=arrival;seen_=true;raw_mm_s_=s.speed_mm_s;
    if(!stationary_quality(s)) { clear_hold();return; }
    const uint64_t now=time_.elapsed_ms;
    if(!stationary_) {
        // At most 3.6 km/h, and within twice the reported speed uncertainty
        // (with a 0.3 m/s floor for very small stationary fluctuations).
        const int enter=std::min(1000,std::max(300,int(s.sacc_mm_s)*2));
        if(s.speed_mm_s>enter) { candidate_=false;return; }
        if(!candidate_) { candidate_=true;candidate_ms_=now;candidate_arrival_us_=arrival; }
        if(now-candidate_ms_>=2000&&arrival-candidate_arrival_us_>=2000000) { stationary_=true;leaving_=false; }
    } else {
        // Unambiguous movement releases immediately at 5 km/h. Below that,
        // require 0.3 s above 3x speed uncertainty (at least 1.8 km/h).
        if(s.speed_mm_s>=1389) { clear_hold();return; }
        const int exit=std::max(500,int(s.sacc_mm_s)*3);
        if(s.speed_mm_s>exit) {
            if(!leaving_) { leaving_=true;leaving_ms_=now;leaving_arrival_us_=arrival; }
            if(now-leaving_ms_>=300&&arrival-leaving_arrival_us_>=300000)clear_hold();
        } else leaving_=false;
    }
}
}
