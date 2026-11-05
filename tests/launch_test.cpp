// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/drag.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace dragy_ref;
using namespace dragy_ref::drag;
static Sample sample(unsigned ms,double v,uint32_t base=100000){
 Sample s{};s.tow_ms=(base+ms)%gps_week_ms;s.fix=3;s.flags=1;s.satellites=12;s.hacc_mm=700;s.sacc_mm_s=100;s.speed_mm_s=int32_t(std::round(v*1000));
 s.valid=3;s.year=2026;s.month=10;s.day=22;s.hour=23;s.minute=59;s.second=59;s.nano=int32_t(ms%1000)*1000000;return s;
}
static void feed(Engine& e,unsigned ms,double v){e.accept(sample(ms,v),10000000+uint64_t(ms)*1000);}
static void warm(Engine& e){e.arm();for(unsigned ms=0;ms<=2000;ms+=100)feed(e,ms,0);}
static void waiting(const Engine& e){for(unsigned i:{0u,4u,5u}){auto& c=e.view().channels[i];assert(c.result.phase==Phase::Waiting&&c.result.sequence==0);}assert(e.view().revision==0);}
static void prefix_preserved(){
 for(unsigned hz:{5,10,25})for(uint32_t base:{100000u,gps_week_ms-2300}){
  Engine e;e.arm();unsigned confirmed_at=0,threshold_at=0;bool pending_seen=false;
  for(unsigned ms=0;ms<=4000;ms+=1000/hz){
   const double t=ms/1000.-2,v=std::max(0.,2*t);
   auto s=sample(ms,v,base);
   if(v>.5&&v<1.389){s.hacc_mm=2500;s.sacc_mm_s=350;s.satellites=6;}
   e.accept(s,10000000+uint64_t(ms)*1000);
   if(v>.5&&v<1.389){pending_seen=true;waiting(e);}
   if(v>=1.389&&!threshold_at)threshold_at=ms;
   if(e.view().channels[0].result.phase==Phase::Running){
    confirmed_at=ms;assert(ms-threshold_at>=300);
    for(unsigned i:{0u,4u,5u}){
     auto& r=e.view().channels[i].result;
     assert(r.phase==Phase::Running&&r.sequence==0&&r.start.tow_ms==(base+2000)%gps_week_ms);
     assert(std::abs(r.elapsed_s-t)<1e-9&&std::abs(r.distance_m-t*t)<1e-9);
     // Quality/sample accounting covers the staged prefix, not just confirmation.
     assert(r.samples>=3&&r.max_hacc_mm==2500&&r.max_sacc_mm_s==350&&r.min_satellites==6);
    }
    break;
   }
  }
  assert(pending_seen&&confirmed_at);
 }
}
static void noise_and_creep(){
 Engine spike;warm(spike);
 for(unsigned ms=2100;ms<20000;ms+=100){feed(spike,ms,ms%2000==100?.65:.03);waiting(spike);}
 Engine creep;warm(creep);
 for(unsigned ms=2100;ms<=6000;ms+=100){feed(creep,ms,.6);waiting(creep);}
 // A rejected candidate cannot resurrect after its confirmation deadline.
 for(unsigned ms=6100;ms<=8000;ms+=100){feed(creep,ms,2);waiting(creep);}
 Engine delayed;warm(delayed);feed(delayed,2100,.6);feed(delayed,2200,.6);feed(delayed,2300,.6);feed(delayed,2400,.6);
 for(unsigned ms=2500;ms<=4000;ms+=100){feed(delayed,ms,.6+(ms-2400)*.004);waiting(delayed);}
 for(unsigned ms=8100;ms<=10000;ms+=100)feed(creep,ms,.03);
 for(unsigned ms=10100;ms<=12000;ms+=100)feed(creep,ms,(ms-10000)*.002);
 assert(creep.view().channels[0].result.phase==Phase::Running);
 assert(creep.view().channels[0].result.start.tow_ms==110000);
}
static void launch_slopes(){
 for(unsigned hz:{5,10,25})for(double acceleration:{.5,.6,1.,4.,20.}){
  Engine e;e.arm();bool confirmed=false;
  for(unsigned ms=0;ms<=8000;ms+=1000/hz){
   const double t=std::max(0.,ms/1000.-2),v=acceleration*t;
   e.accept(sample(ms,v),10000000+uint64_t(ms)*1000);
   if(e.view().channels[0].result.phase==Phase::Running){
    auto& r=e.view().channels[0].result;assert(std::abs(r.start.tow_ms-102000)<.001);
    assert(std::abs(r.distance_m-.5*acceleration*t*t)<.00001);
    assert(std::abs(r.elapsed_s-t)<1e-9);confirmed=true;break;
   }
  }
  if(!confirmed)fprintf(stderr,"No confirmation hz=%u acceleration=%g\n",hz,acceleration);
  assert(confirmed);
 }
}
static void sustained_uncertainty(){
 Engine e;warm(e);
 for(unsigned ms=2100;ms<=2200;ms+=100){auto s=sample(ms,(ms-2000)*.006);s.sacc_mm_s=500;e.accept(s,10000000+uint64_t(ms)*1000);waiting(e);}
 for(unsigned ms=2300;ms<=2900;ms+=100){auto s=sample(ms,1.4);s.sacc_mm_s=100;e.accept(s,10000000+uint64_t(ms)*1000);waiting(e);}
 // 1.4 m/s is above 5 km/h but below 3 times the worst reported uncertainty.
 for(unsigned ms=3000;ms<=3300;ms+=100){auto s=sample(ms,1.6);s.sacc_mm_s=100;e.accept(s,10000000+uint64_t(ms)*1000);if(ms<3300)waiting(e);}
 auto& r=e.view().channels[0].result;assert(r.phase==Phase::Running&&r.start.tow_ms==102000&&r.max_sacc_mm_s==500);
 Engine reset;warm(reset);feed(reset,2100,.6);feed(reset,2200,1.5);feed(reset,2300,1.5);feed(reset,2400,1.5);waiting(reset);
 feed(reset,2500,1.2);waiting(reset); // Below the confirmation threshold resets dwell.
 for(unsigned ms=2600;ms<=2900;ms+=100){feed(reset,ms,1.5);if(ms<2900)waiting(reset);}
 assert(reset.view().channels[0].result.phase==Phase::Running&&reset.view().channels[0].result.start.tow_ms==102000);
 // The GPS dwell cannot complete from duplicate epochs or a compressed host burst.
 Engine burst;warm(burst);feed(burst,2100,.6);feed(burst,2200,1.5);
 for(unsigned ms=2300;ms<=2500;ms+=100)burst.accept(sample(ms,1.5),12200000+(ms-2200));
 waiting(burst);burst.accept(sample(2500,1.5),12500000);waiting(burst);
 burst.accept(sample(2600,1.5),12600000);assert(burst.view().channels[0].result.phase==Phase::Running);
}
static void cancel_on_boundaries(){
 for(unsigned fault=0;fault<11;++fault){
  Engine e;warm(e);feed(e,2100,.6);feed(e,2200,1.2);waiting(e);
  auto s=sample(2300,1.8);auto arrival=12300000ull;
  switch(fault){
   case 0:s.fix=0;s.flags=0;break;
   case 1:s.satellites=5;break;
   case 2:s.sacc_mm_s=501;break;
   case 3:s.hacc_mm=5001;break;
   case 4:s.tow_ms=102500;arrival=12500000;break;
   case 5:s.tow_ms=90000;break;
   case 6:arrival=12800000;break;
   case 7:s.speed_mm_s=50000;break;
   case 8:e.interrupt();break;
   case 9:e.disarm();e.arm();break;
   case 10:e.poll(12700001);break;
  }
  e.accept(s,arrival);waiting(e);
  for(unsigned ms=3000;ms<6000;ms+=100){feed(e,ms,2);waiting(e);}
 }
 // A fall below the launch threshold consumes the candidate and needs a fresh standstill.
 Engine bounce;warm(bounce);feed(bounce,2100,.6);feed(bounce,2200,.4);feed(bounce,2300,.8);
 for(unsigned ms=2400;ms<5000;ms+=100){feed(bounce,ms,2);waiting(bounce);}
}
int main(){prefix_preserved();noise_and_creep();launch_slopes();sustained_uncertainty();cancel_on_boundaries();puts("PASS launch confirmation: noise/creep rejection, 0.5-20 m/s2 launch slopes, bounded uncertainty-aware dwell, raw onset/distance/quality retained at 5/10/25 Hz and GPS rollover, host/duplicate guards, 11 cancellation boundaries and fresh-standstill rearm");}
