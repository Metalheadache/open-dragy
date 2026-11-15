// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "dragy_ref/timing.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace dragy_ref;
using drag::TimingGate25;
static Sample sample(unsigned ms){
    Sample s{};s.fix=3;s.flags=1;s.satellites=8;s.hacc_mm=800;s.sacc_mm_s=100;
    s.tow_ms=100000+ms;s.speed_mm_s=std::max(0,int(ms)-2000)*4;return s;
}
static uint64_t arrival(unsigned ms){return 10000000+ms*1000ull;}
int main(){
    for(unsigned rate:{40u,100u,200u}){
        TimingGate25 t;t.arm();t.configure(false);
        for(unsigned ms=0;ms<18000;ms+=rate)t.accept(sample(ms),arrival(ms));
        assert((t.view().channels[0].result.phase==drag::Phase::Complete)==(rate==40));
        assert(t.ready(arrival(17960))==(rate==40));
    }
    for(unsigned gap:{80u,120u,160u,200u}){
        TimingGate25 t;t.arm();t.configure(false);
        const unsigned start=8800,end=start+gap;
        for(unsigned ms=0;ms<=18000;ms+=40){
            if(ms>start&&ms<end)continue;
            t.accept(sample(ms),arrival(ms));
            if(ms==end){assert(t.awaiting_confirmation());assert(!t.ready(arrival(ms)));assert(t.view().channels[0].result.phase!=drag::Phase::Complete);}
        }
        assert(t.bridged==1&&t.view().channels[0].result.phase==drag::Phase::Complete);
        assert(std::abs(t.view().channels[0].result.elapsed_s-100/3.6/4)<.005);
    }
    for(unsigned fault=0;fault<8;++fault){
        TimingGate25 t;t.arm();t.configure(false);
        for(unsigned ms=0;ms<=8800;ms+=40)t.accept(sample(ms),arrival(ms));
        t.accept(sample(9000),arrival(9000));assert(t.awaiting_confirmation());
        auto s=sample(9040);
        if(fault==0)s.fix=2;if(fault==1)s.satellites=5;if(fault==2)s.sacc_mm_s=501;
        if(fault==3)s.hacc_mm=5001;if(fault==4)s.tow_ms+=40;
        if(fault==5){t.poll(arrival(9600));}
        if(fault==6){t.interrupt();}
        if(fault==7)s.speed_mm_s+=5000; // valid quality, impossible confirming acceleration
        t.accept(s,arrival(fault==5?9600:9040));
        assert(t.view().channels[0].result.phase!=drag::Phase::Complete);
    }
    {TimingGate25 t;t.configure(false);for(unsigned ms=0;ms<=400;ms+=40)t.accept(sample(ms),arrival(ms));
     assert(t.ready(arrival(400)));t.accept(sample(640),arrival(640));assert(!t.ready(arrival(640))&&t.rejected==1);}
    {TimingGate25 t;t.configure(false);for(unsigned ms=0;ms<=400;ms+=40)t.accept(sample(ms),arrival(ms));
     t.accept(sample(480),arrival(480));t.accept(sample(520),arrival(520));assert(t.bridged==1);
     t.accept(sample(600),arrival(600));assert(t.rejected==1&&!t.ready(arrival(600)));}
    {TimingGate25 t;t.arm(); // pending setup may observe but cannot admit timing
     for(unsigned ms=0;ms<=2000;ms+=40)t.accept(sample(ms),arrival(ms));
     assert(t.clean_intervals()>=8&&!t.ready(arrival(2000)));
     t.configure(false);for(unsigned ms=2040;ms<=2320;ms+=40)t.accept(sample(ms),arrival(ms));
     assert(!t.ready(arrival(2320)));t.accept(sample(2360),arrival(2360));assert(t.ready(arrival(2360)));
     t.poll(arrival(3000));assert(!t.ready(arrival(3000)));}
    {TimingGate25 t;t.configure(false);for(unsigned ms=0;ms<=280;ms+=40)t.accept(sample(ms),arrival(ms));
     t.accept(sample(320),arrival(320),1);assert(t.clean_intervals()==0);
     for(unsigned ms=360;ms<=640;ms+=40)t.accept(sample(ms),arrival(ms),1);
     assert(t.ready(arrival(640)));t.accept(sample(680),arrival(680),2);assert(t.ready(arrival(680)));}
    {TimingGate25 t;t.configure(false);for(unsigned ms=0;ms<=800;ms+=40){auto s=sample(ms);s.tow_ms=(gps_week_ms-400+ms)%gps_week_ms;t.accept(s,arrival(ms));}assert(t.ready(arrival(800)));}
    {TimingGate25 t;t.configure(false);for(unsigned ms=0;ms<=800;ms+=40)t.accept(sample(ms),arrival(ms),0,false);assert(!t.ready(arrival(800)));}
    {TimingGate25 t;t.configure(false);for(unsigned ms=0;ms<=400;ms+=40)t.accept(sample(ms),arrival(ms));
     assert(t.clean_intervals()>=8);t.accept(sample(400),arrival(440));assert(t.clean_intervals()==0);
     assert(!t.ready(arrival(680)));}
    {TimingGate25 t;t.arm();t.configure(false);
     auto delivered=[](unsigned ms){return arrival(ms)-std::min(1000u,ms>1000?ms-1000:0u)*480ull;};
     for(unsigned ms=0;ms<=8800;ms+=40)t.accept(sample(ms),delivered(ms));
     t.accept(sample(9000),delivered(9000));assert(t.awaiting_confirmation());
     t.accept(sample(9040),delivered(9000)); // receiver progression now exceeds host by >500ms
     assert(t.view().channels[0].result.phase!=drag::Phase::Complete);}
    auto a=sample(0),b=a;a.speed_mm_s=b.speed_mm_s=50000;
    assert(timing::short_gap(a,b,200));b.speed_mm_s=50001;assert(!timing::short_gap(a,b,200));
    b=a;assert(!timing::short_gap(a,b,201));b.lat_e7=901;assert(!timing::short_gap(a,b,80));
    a.lon_e7=1799999990;b.lon_e7=-1799999990;b.lat_e7=0;assert(timing::short_gap(a,b,80));
    puts("PASS strict25 startup, pending/queued input, errors, rollover, 5/10 Hz rejection; 80-200ms provisional finishes, confirmation quality, dense/stale/reset rejection and 10m bounds");
}
