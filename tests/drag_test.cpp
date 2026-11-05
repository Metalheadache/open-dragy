// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/drag.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
using namespace dragy_ref;
using namespace dragy_ref::drag;
static Sample sample(uint32_t tow,double mps){Sample s{};s.tow_ms=tow%gps_week_ms;s.fix=3;s.flags=1;s.satellites=16;s.hacc_mm=600;s.sacc_mm_s=80;s.speed_mm_s=int32_t(std::round(mps*1000));s.lat_e7=13000000;s.lon_e7=1038000000;s.msl_mm=10000;s.valid=3;s.year=2026;s.month=10;s.day=15;s.hour=4;s.nano=int32_t(tow%1000)*1000000;return s;}
static void close(double actual,double expected,double tolerance=.002){if(std::abs(actual-expected)>tolerance){fprintf(stderr,"Actual %.9f; expected %.9f; tolerance %.6f\n",actual,expected,tolerance);assert(false);}}
static void warm(Engine& e,double v=0){e.arm();for(unsigned t=0;t<=2000;t+=40)e.accept(sample(100000+t,v),10000000+t*1000);}
static void ramp(Engine& e,unsigned end=6000){for(unsigned t=2040;t<=end;t+=40)e.accept(sample(100000+t,(t-2000)*.004),10000000+t*1000);}
static void all_tests(unsigned step,bool rollover,bool jitter,std::ofstream* csv=nullptr){
 Engine e;e.arm();const double accel=jitter?3.713:3.7,brake=jitter?7.317:7.3,peak=92.;const double up=peak/accel;const uint32_t base=rollover?gps_week_ms-1000:100000;
 for(unsigned ms=0;ms<45000;ms+=step){
  const double t=ms/1000.-2;double v=t<=0?0:t<up?accel*t:std::max(0.,peak-brake*(t-up));
  auto s=sample((base+ms)%gps_week_ms,v);e.accept(s,10000000+ms*1000+(jitter?(ms/step%3)*9000:0));
  if(csv && ms%200==0)*csv<<ms<<','<<std::fixed<<std::setprecision(6)<<v<<','<<int(e.view().channels[0].result.phase)<<','<<e.view().channels[0].result.elapsed_s<<'\n';
 }
 for(unsigned i=0;i<count;i++){
  const auto& def=definitions[i];const auto& c=e.view().channels[i];assert(c.result.phase==Phase::Complete);assert(c.result.fault==Fault::None);assert(c.result.samples>1);
  const double expected=def.kind==Kind::Distance?std::sqrt(2*def.metres/accel):def.kind==Kind::Braking?def.start_mps/brake:(def.end_mps-def.start_mps)/accel;
  close(c.result.elapsed_s,expected);if(def.kind==Kind::Distance)close(c.result.distance_m,def.metres,1e-9);
  assert(c.result.min_satellites==16);
 }
 e.arm();assert(e.view().channels[0].result.phase==Phase::Waiting);
}
static void faults(){
 for(Fault f:{Fault::Fix,Fault::Accuracy,Fault::Satellites,Fault::Gap,Fault::Clock,Fault::Stale,Fault::Interrupted,Fault::Aborted}){
  Engine e;warm(e);ramp(e,4000);assert(e.view().channels[0].result.phase==Phase::Running);
  auto s=sample(104040,8.16);
  if(f==Fault::Fix)s.fix=2;
  if(f==Fault::Accuracy)s.sacc_mm_s=501;
  if(f==Fault::Satellites)s.satellites=5;
  if(f==Fault::Gap)s.tow_ms=104080; // A missing established 25 Hz epoch, even within the absolute 200 ms limit.
  if(f==Fault::Clock)s.tow_ms=90000;
  if(f==Fault::Stale)e.poll(14500001);
  else if(f==Fault::Interrupted)e.interrupt();
  else if(f==Fault::Aborted)e.disarm();
  else e.accept(s,14040000);
  assert(e.view().channels[0].result.phase==Phase::Invalid);assert(e.view().channels[0].result.fault==f);
  for(unsigned t=4080;t<16000;t+=40)e.accept(sample(100000+t,(t-2000)*.004),10000000+t*1000);
  assert(e.view().channels[0].result.phase==Phase::Invalid); // Must never resume an interrupted run.
 }
 Engine no_start;no_start.arm();for(unsigned t=0;t<12000;t+=40)no_start.accept(sample(100000+t,30+t*.002),10000000+t*1000);
 assert(no_start.view().channels[0].result.phase==Phase::Waiting);assert(no_start.view().channels[4].result.phase==Phase::Waiting);
 Engine timed;warm(timed);ramp(timed,3000);for(unsigned t=3040;t<124000;t+=40)timed.accept(sample(100000+t,4),10000000+t*1000);
 assert(timed.view().channels[0].result.fault==Fault::Timeout);
 Engine duplicates;warm(duplicates);ramp(duplicates,4000);auto before=duplicates.view().channels[0].result.elapsed_s;
 duplicates.accept(sample(104000,100),14001000);duplicates.accept(sample(103960,100),14002000);close(duplicates.view().channels[0].result.elapsed_s,before,1e-12);
 duplicates.poll(14500001);assert(duplicates.view().channels[0].result.fault==Fault::Stale);
}
static void independent_results(){
 Engine e;warm(e);ramp(e,10000);assert(e.view().channels[0].result.phase==Phase::Complete);
 e.interrupt();assert(e.view().channels[0].result.phase==Phase::Complete);assert(e.view().channels[4].result.phase==Phase::Invalid);
 Engine disabled;for(unsigned t=0;t<12000;t+=40)disabled.accept(sample(100000+t,t*.005),10000000+t*1000);assert(disabled.view().revision==0);
 // Flat standstill noise cannot trigger launch. Braking noise cannot finish without a decreasing zero estimate.
 Engine burst;burst.arm();for(unsigned t=0;t<16000;t+=40)burst.accept(sample(100000+t,t<=2000?0:(t-2000)*.004),10000000+t);assert(burst.view().channels[0].result.phase==Phase::Waiting);
 Engine noise;noise.arm();for(unsigned t=0;t<6000;t+=40)noise.accept(sample(100000+t,.1+(t%120)*.001),10000000+t*1000);assert(noise.view().channels[0].result.phase==Phase::Waiting);
 // Starting above a rolling threshold does not fabricate a crossing at arm time.
 Engine rolling;warm(rolling,30);for(unsigned t=2040;t<6000;t+=40)rolling.accept(sample(100000+t,30+(t-2000)*.005),10000000+t*1000);assert(rolling.view().channels[1].result.phase==Phase::Waiting);
}
static void distance_profiles(){
 for(bool braking:{false,true}){
  Engine e;e.arm();
  for(unsigned ms=0;ms<90000;ms+=40){
   double t=ms/1000.-2,v=t<=0?0:t<15?2*t:braking&&t>18?std::max(0.,30-.5*(t-18)):30;
   e.accept(sample(100000+ms,v),10000000+ms*1000);
  }
  for(unsigned i:{4u,5u}){
   const auto& r=e.view().channels[i].result;assert(r.phase==Phase::Complete);
   double expected=braking?18+2*(definitions[i].metres-315)/(30+std::sqrt(900-(definitions[i].metres-315))):15+(definitions[i].metres-225)/30;
   close(r.elapsed_s,expected,.002);close(r.distance_m,definitions[i].metres,1e-9);
  }
 }
}
static void guards_and_stop(){
 Engine creep;warm(creep);
 for(unsigned ms=2040;ms<=5000;ms+=40)creep.accept(sample(100000+ms,.4),10000000+ms*1000);
 for(unsigned ms=5040;ms<16000;ms+=40)creep.accept(sample(100000+ms,.4+(ms-5000)*.004),10000000+ms*1000);
 assert(creep.view().channels[0].result.phase==Phase::Waiting);
 Engine spike;warm(spike);ramp(spike,4000);spike.accept(sample(104040,100),14040000);assert(spike.view().channels[0].result.fault==Fault::Dynamics);
 Engine brake;warm(brake,30);
 for(unsigned ms=2040;ms<=5760;ms+=40){
  double v=std::max(0.,30-(ms-2000)*.01);if(ms==5240)v=.4;
  brake.accept(sample(100000+ms,v),10000000+ms*1000);
 }
 assert(brake.view().channels[6].result.phase==Phase::Running);
 brake.accept(sample(105800,0),15800000);assert(brake.view().channels[6].result.phase==Phase::Complete);

}
int main(int argc,char** argv){
 std::ofstream csv;if(argc==2){csv.open(argv[1]);assert(csv);csv<<"trace_ms,speed_mps,0_100_phase,0_100_elapsed_s\n";}
 all_tests(40,false,false,csv?&csv:nullptr);all_tests(100,false,true);all_tests(200,true,false);all_tests(40,true,true);
 faults();independent_results();distance_profiles();guards_and_stop();
 puts("PASS: all 7 Drag tests vs analytic motion at 5/10/25 Hz; interpolation, exact mile distances, braking zero confirmation, GPS week rollover, arrival jitter, disarmed/no-launch/no-crossing guards, duplicate/backward epochs, loss/reset/stale/cancel/timeout invalidation, independent results");
}
