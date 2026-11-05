// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/drag.hpp"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
using namespace dragy_ref;using namespace dragy_ref::drag;
static double launch_speed(double t){return t<=0?0:t<4?2*t+.3*t*t:12.8+4.4*(t-4);}
static double launch_time(double v){return v<12.8?(-2+std::sqrt(4+1.2*v))/.6:4+(v-12.8)/4.4;}
static double launch_distance(double t){return t<=0?0:t<4?t*t+.1*t*t*t:22.4+12.8*(t-4)+2.2*(t-4)*(t-4);}
static double distance_time(double metres){double lo=0,hi=30;for(unsigned i=0;i<60;i++){double mid=(lo+hi)/2;if(launch_distance(mid)<metres)lo=mid;else hi=mid;}return (lo+hi)/2;}
static double brake_time(double v){return (-6+std::sqrt(36+.6*(92-v)))/.3;}
static Sample sample(unsigned ms,double speed){Sample s{};s.tow_ms=100000+ms;s.fix=3;s.flags=1;s.satellites=14;s.hacc_mm=800;s.sacc_mm_s=100;s.speed_mm_s=int32_t(std::round(std::max(0.,speed)*1000));return s;}
int main(){
 std::filesystem::create_directories(".cache/drag-refinement");std::ofstream out(".cache/drag-refinement/curved-profiles.csv");out<<std::setprecision(12)<<"hz,noise_mps,test,expected_s,actual_s,error_s\n";
 for(unsigned hz:{5,10,25})for(double noise:{0.,.02,.05}){
  Engine e;e.arm();double peak_at=launch_time(92),stop_at=brake_time(0);
  for(unsigned ms=0;ms<=50000;ms+=1000/hz){double t=ms/1000.-2,v=t<peak_at?launch_speed(t):std::max(0.,92-6*(t-peak_at)-.15*(t-peak_at)*(t-peak_at));
   if(t>0&&t<peak_at+stop_at)v=std::max(0.,v+noise*std::sin(ms*.013));else v=noise*.5*(1+std::sin(ms*.013));
   e.accept(sample(ms,v),10000000+ms*1000+(ms/(1000/hz)%3)*2000);
  }
  for(unsigned i=0;i<count;i++){
   auto& r=e.view().channels[i].result;auto& d=definitions[i];if(r.phase!=Phase::Complete){fprintf(stderr,"Curved profile fail hz=%u noise=%g test=%u phase=%u fault=%s\n",hz,noise,i,unsigned(r.phase),fault_text(r.fault));assert(false);}
   double expected=d.kind==Kind::Distance?distance_time(d.metres):d.kind==Kind::Braking?brake_time(0)-brake_time(d.start_mps):launch_time(d.end_mps)-launch_time(d.start_mps);
   out<<hz<<','<<noise<<','<<i<<','<<expected<<','<<r.elapsed_s<<','<<r.elapsed_s-expected<<'\n';
   assert(std::abs(r.elapsed_s-expected)<.05);assert(std::isfinite(r.distance_m)&&r.distance_m>=0);
  }
 }
 // Low-quality real-receiver conditions stay rejected, never relaxed to create times.
 Engine poor;poor.arm();for(unsigned ms=0;ms<20000;ms+=100){auto s=sample(ms,launch_speed(ms/1000.-2));s.satellites=5;s.hacc_mm=6000;s.sacc_mm_s=500;poor.accept(s,10000000+ms*1000);}for(auto& c:poor.view().channels)assert(c.result.phase!=Phase::Complete);
 puts("PASS Drag refinement: 63 curved acceleration/braking/distance results at 5/10/25Hz with 0/0.02/0.05m/s noise and arrival jitter; no quality-gate relaxation; errors recorded, 50ms estimator bound");
}
