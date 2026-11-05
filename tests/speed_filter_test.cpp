// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/speed_filter.hpp"
#include <cassert>
#include <cstdio>
#include <cmath>
using namespace dragy_ref;
static Sample good(){Sample s{};s.fix=3;s.flags=1;s.satellites=8;s.hacc_mm=6000;s.sacc_mm_s=500;return s;}
int main(){
    DisplaySpeedHold filter;auto s=good();uint64_t host=1000000;uint32_t tow=100000;
    auto feed=[&](int speed,unsigned dt=100){host+=uint64_t(dt)*1000;tow+=dt;s.tow_ms=tow;s.speed_mm_s=speed;filter.accept(s,host);};
    for(int i=0;i<80;++i){int raw=300+(i*71)%650;feed(raw);assert(s.speed_mm_s==raw);if(i>=20)assert(filter.speed_mm_s(host)==0);else assert(filter.speed_mm_s(host)==raw);}
    assert(filter.stationary(host));
    // A single burst/weak low-speed spike does not chatter out of zero.
    feed(1050);assert(filter.stationary(host));feed(600);assert(filter.stationary(host));
    feed(1500);assert(!filter.stationary(host));assert(filter.speed_mm_s(host)==1500); // immediate move-off
    // Accurately measured creeping stays visible, including after a noisy stop.
    filter.reset();s.sacc_mm_s=50;
    for(int i=0;i<40;++i){feed(600);assert(filter.speed_mm_s(host)==600&&!filter.stationary(host));}
    s.sacc_mm_s=500;for(int i=0;i<25;++i)feed(600);assert(filter.stationary(host));
    s.sacc_mm_s=50;for(int i=0;i<3;++i){feed(600);assert(filter.stationary(host));}feed(600);assert(!filter.stationary(host));
    auto hold=[&]{filter.reset();s=good();for(int i=0;i<25;++i)feed(600);assert(filter.stationary(host));};
    hold();assert(!filter.stationary(host+500001));assert(filter.speed_mm_s(host+500001)==600);
    hold();s.flags=0;feed(600);assert(!filter.stationary(host));
    hold();s.fix=2;feed(600);assert(!filter.stationary(host));
    hold();s.hacc_mm=15001;feed(600);assert(!filter.stationary(host));
    hold();s.sacc_mm_s=1001;feed(600);assert(!filter.stationary(host));
    hold();s.sacc_mm_s=0;feed(600);assert(!filter.stationary(host));
    hold();s.satellites=3;feed(600);assert(!filter.stationary(host));
    hold();feed(600,600);assert(!filter.stationary(host));
    // Duplicates/backward epochs and UI refreshes cannot accumulate a dwell.
    filter.reset();s=good();feed(600);auto t0=s.tow_ms;
    for(int i=0;i<100;++i){host+=10000;filter.accept(s,host);assert(!filter.stationary(host));}
    s.tow_ms=t0-100;filter.accept(s,host);assert(!filter.stationary(host));
    // Buffered receiver epochs in one host callback do not count as real dwell.
    filter.reset();s=good();for(int i=0;i<40;++i){s.tow_ms=10000+i*100;s.speed_mm_s=600;filter.accept(s,host);assert(!filter.stationary(host));}
    hold();s.tow_ms=gps_week_ms;filter.accept(s,host);assert(!filter.stationary(host));
    hold();s.tow_ms=100;filter.accept(s,host);assert(!filter.stationary(host));
    hold();filter.accept(s,host-1);assert(!filter.stationary(host-1));
    // Valid GPS week rollover preserves a real, continuous stationary dwell.
    filter.reset();s=good();for(unsigned i=0;i<35;++i){host+=100000;s.tow_ms=(gps_week_ms-1000+i*100)%gps_week_ms;s.speed_mm_s=600;filter.accept(s,host);}assert(filter.stationary(host));
    // 25 Hz works independently of the UI refresh cadence.
    filter.reset();s=good();for(unsigned i=0;i<100;++i){host+=40000;s.tow_ms=50000+i*40;s.speed_mm_s=500;filter.accept(s,host);}assert(filter.stationary(host));
    puts("PASS Presentation speed filter: 2s dwell, noise hysteresis, immediate/confirmed move-off, accurate creep, raw preservation, invalid/poor fixes, host/receiver gaps, duplicates/backward/reset/bursts, stale expiry, week rollover and 25Hz");
}
