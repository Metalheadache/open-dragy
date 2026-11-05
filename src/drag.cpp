// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/drag.hpp"
#include <algorithm>
#include <cmath>
namespace dragy_ref::drag {
constexpr double kmh(double v){return v/3.6;}
const std::array<Definition,count> definitions{{
 {"0–100 KM/H",Kind::Acceleration,0,kmh(100),0},{"60–160 KM/H",Kind::Acceleration,kmh(60),kmh(160),0},
 {"100–200 KM/H",Kind::Acceleration,kmh(100),kmh(200),0},{"200–300 KM/H",Kind::Acceleration,kmh(200),kmh(300),0},
 {"1/4 MILE",Kind::Distance,0,0,402.336},{"1/2 MILE",Kind::Distance,0,0,804.672},
 {"100–0 KM/H",Kind::Braking,kmh(100),0,0}
}};
const char* fault_text(Fault f){switch(f){
 case Fault::None:return "None";case Fault::Fix:return "No 3D fix";case Fault::Accuracy:return "GPS accuracy";
 case Fault::Satellites:return "Too few satellites";case Fault::Gap:return "GPS sample gap";case Fault::Clock:return "GPS clock reset";
 case Fault::Stale:return "GPS stream stale";case Fault::Interrupted:return "Link interrupted";case Fault::Aborted:return "Run aborted";
 case Fault::Timeout:return "Run timeout";case Fault::Dynamics:return "GPS speed jump";case Fault::Latency:return "GPS delivery burst";}return "Unknown";}
const char* phase_text(Phase p){switch(p){case Phase::Waiting:return "WAITING";case Phase::Running:return "RUNNING";case Phase::Complete:return "COMPLETE";case Phase::Invalid:return "INVALID";}return "UNKNOWN";}
static Point point(const Sample& a,const Sample& b,double fraction,double timestamp,double /*b_ms*/){
 Point p;p.tow_ms=std::fmod(timestamp+gps_week_ms,gps_week_ms);
 double f=std::clamp(fraction,0.,1.),lon_delta=(b.lon_e7-double(a.lon_e7))/1e7;
 if(lon_delta>180)lon_delta-=360;
 if(lon_delta<-180)lon_delta+=360;
 p.lat_deg=(a.lat_e7+(b.lat_e7-double(a.lat_e7))*f)/1e7;p.lon_deg=a.lon_e7/1e7+lon_delta*f;
 if(p.lon_deg>180)p.lon_deg-=360;
 if(p.lon_deg<-180)p.lon_deg+=360;
 p.msl_m=(a.msl_mm+(b.msl_mm-double(a.msl_mm))*f)/1000.;
 return p;
}
void Engine::reset_continuity(){previous_good_=false;stationary_=false;still_since_=-1;cadence_=0;launch_={};for(auto& r:runs_)r.eligible=false;}
void Engine::invalidate(Fault reason){
 for(auto& ch:view_.channels)if(ch.result.phase==Phase::Running){ch.result.phase=Phase::Invalid;ch.result.fault=reason;ch.result.sequence=++view_.revision;}
 reset_continuity();
}
void Engine::interrupt(Fault reason){invalidate(reason);time_.reset();arrival_=0;}
void Engine::arm(){for(auto& c:view_.channels)c.result=Result{};runs_={};view_.armed=true;time_.reset();arrival_=0;reset_continuity();}
void Engine::disarm(){invalidate(Fault::Aborted);view_.armed=false;time_.reset();arrival_=0;}
void Engine::poll(uint64_t now){if(view_.armed&&arrival_&&now>=arrival_&&now-arrival_>500000)interrupt(Fault::Stale);}
void Engine::finish(unsigned i,double end,double distance,const Point& p){
 auto& c=view_.channels[i];auto& out=c.result;out.elapsed_s=(end-runs_[i].start_ms)/1000.;out.distance_m=distance;out.finish=p;
 if(out.elapsed_s<=0||out.elapsed_s>120){out.phase=Phase::Invalid;out.fault=Fault::Timeout;}
 else{out.phase=Phase::Complete;out.fault=Fault::None;}
 out.sequence=++view_.revision;
}
void Engine::update_launch(const Sample& s,uint64_t arrival,double now,double dt,double v,double pv,double a,bool crossing){
 const double uncertainty=std::max({launch_.result.max_sacc_mm_s,s.sacc_mm_s,previous_.sacc_mm_s})/1000.;
 const double threshold=std::max(1.389,3*uncertainty),age=now-launch_.detected_ms;
 // Do not let a low-speed noise plateau become the timestamp of a later real
 // launch. Below confirmation speed, keep the original 0.5 m/s² launch growth
 // with 0.1 m/s tolerance, evaluated only after the first 300 ms.
 const bool stalled=launch_.pending&&v<threshold&&age>=300&&v+.1<launch_.initial_mps+.5*age/1000.;
 if(launch_.pending&&(v<=.5||age>3000||stalled)){
  launch_={};stationary_=false;still_since_=v<=.3?now:-1;
 }
 if(!launch_.pending&&crossing){
  bool waiting=false;for(unsigned i:{0u,4u,5u})waiting|=view_.channels[i].result.phase==Phase::Waiting;
  if(waiting){
   launch_={};launch_.pending=true;launch_.detected_ms=now;launch_.initial_mps=v;
   launch_.start_ms=previous_ms_-pv/a*1000.;
   auto& out=launch_.result;out.phase=Phase::Running;
   out.start=point(previous_,s,(launch_.start_ms-previous_ms_)/(dt*1000),previous_.tow_ms+(launch_.start_ms-previous_ms_),double(previous_.tow_ms)+dt*1000);
   out.distance_m=.5*pv*(previous_ms_-launch_.start_ms)/1000.;
  }
 }
 if(!launch_.pending)return;
 if(v>=threshold){
  if(launch_.confirm_ms<0){launch_.confirm_ms=now;launch_.confirm_arrival_us=arrival;}
 }else launch_.confirm_ms=-1;
 if(launch_.confirm_ms>=0&&now-launch_.confirm_ms>=300&&arrival>=launch_.confirm_arrival_us&&arrival-launch_.confirm_arrival_us>=300000){
  // The staged result ends at the previous sample. The ordinary loop below
  // integrates this confirmation segment exactly once, using its raw speed.
  for(unsigned i:{0u,4u,5u})if(view_.channels[i].result.phase==Phase::Waiting){
   view_.channels[i].result=launch_.result;runs_[i]={};runs_[i].start_ms=launch_.start_ms;
  }
  launch_={};return;
 }
 auto& out=launch_.result;++out.samples;
 out.elapsed_s=(now-launch_.start_ms)/1000.;out.distance_m+=std::max(0.,(pv+v)*.5*dt);
 out.max_hacc_mm=std::max({out.max_hacc_mm,s.hacc_mm,previous_.hacc_mm});
 out.max_sacc_mm_s=std::max({out.max_sacc_mm_s,s.sacc_mm_s,previous_.sacc_mm_s});
 out.min_satellites=std::min({out.min_satellites,s.satellites,previous_.satellites});
}
void Engine::accept(const Sample& s,uint64_t arrival){
 if(!view_.armed)return;
 if(arrival_ && (arrival<arrival_||arrival-arrival_>500000))interrupt(Fault::Stale);
 auto epoch=time_.accept(s.tow_ms);
 if(epoch==Epoch::Duplicate||epoch==Epoch::Backward)return;
 arrival_=arrival;
 if(epoch==Epoch::Reset||epoch==Epoch::Invalid){interrupt(Fault::Clock);return;}
 const double now=double(time_.elapsed_ms),dt=time_.delta_ms/1000.,v=s.speed_mm_s/1000.;
 Fault quality=Fault::None;
 if(!s.position_valid()||s.fix!=3)quality=Fault::Fix;
 else if(s.hacc_mm>5000||s.sacc_mm_s==0||s.sacc_mm_s>500)quality=Fault::Accuracy;
 else if(s.satellites<6)quality=Fault::Satellites;
 if(quality!=Fault::None){invalidate(quality);return;}
 if(previous_good_ && (time_.delta_ms>200 || (cadence_>0 && time_.delta_ms>cadence_*1.5))){invalidate(Fault::Gap);}
 if(previous_good_&&arrival>=host_anchor_us_&&now-gps_anchor_ms_>(arrival-host_anchor_us_)/1000.+500){invalidate(Fault::Latency);}
 if(!previous_good_){host_anchor_us_=arrival;gps_anchor_ms_=now;previous_=s;previous_ms_=now;previous_good_=true;if(v<=.3)still_since_=now;return;}
 if(epoch!=Epoch::Forward||dt<=0)return;
 cadence_=cadence_==0?time_.delta_ms:cadence_*.9+time_.delta_ms*.1;
 const double pv=previous_.speed_mm_s/1000.,a=(v-pv)/dt;
 if(std::abs(a)>30){invalidate(Fault::Dynamics);return;}
 if(v<=.3){last_still_ms_=now;if(still_since_<0)still_since_=now;if(now-still_since_>=1000)stationary_=true;}else still_since_=-1;
 if(stationary_&&now-last_still_ms_>1000)stationary_=false;
 // mm/s divided by ms is m/s². Use the integer speed difference for
 // acceptance at the exact 0.5/20 boundaries; keep the original estimator.
 const double launch_a=(s.speed_mm_s-previous_.speed_mm_s)/double(time_.delta_ms);
 const bool launch=stationary_&&pv<=.5&&v>.5&&launch_a>=.5&&launch_a<=20&&pv<=launch_a;
 update_launch(s,arrival,now,dt,v,pv,a,launch);
 for(unsigned i=0;i<count;i++){
  auto& out=view_.channels[i].result;auto& run=runs_[i];const auto& def=definitions[i];
  if(out.phase==Phase::Complete||out.phase==Phase::Invalid)continue;
  double segment_start=0;
  if(out.phase==Phase::Waiting){
   if(def.start_mps==0&&def.kind!=Kind::Braking)continue;
   bool start=false;double when=now;
   if(def.kind==Kind::Acceleration){if(pv<def.start_mps-.3)run.eligible=true;if(run.eligible&&pv<def.start_mps&&v>=def.start_mps&&v>pv){when=previous_ms_+(def.start_mps-pv)/(v-pv)*dt*1000;start=true;}}
   else {if(pv>def.start_mps+.3)run.eligible=true;if(run.eligible&&pv>def.start_mps&&v<=def.start_mps&&v<pv){when=previous_ms_+(def.start_mps-pv)/(v-pv)*dt*1000;start=true;}}
   if(!start)continue;
   out=Result{};out.phase=Phase::Running;run.start_ms=when;run.stop_ms=-1;
   out.start=point(previous_,s,(when-previous_ms_)/(dt*1000),previous_.tow_ms+(when-previous_ms_),double(previous_.tow_ms)+dt*1000);
   segment_start=(when-previous_ms_)/1000.;
   // The launch estimate may precede the previous sample; include its zero-to-pv triangle.
   if(segment_start<0){out.distance_m=.5*pv*(-segment_start);segment_start=0;}
  }
  ++out.samples;out.max_hacc_mm=std::max({out.max_hacc_mm,s.hacc_mm,previous_.hacc_mm});out.max_sacc_mm_s=std::max({out.max_sacc_mm_s,s.sacc_mm_s,previous_.sacc_mm_s});out.min_satellites=std::min({out.min_satellites,s.satellites,previous_.satellites});
  out.elapsed_s=(now-run.start_ms)/1000.;
  if(out.elapsed_s>120){out.phase=Phase::Invalid;out.fault=Fault::Timeout;out.sequence=++view_.revision;continue;}
  const double start_v=pv+a*segment_start,span=dt-segment_start;
  const double segment_distance=std::max(0.,(start_v+v)*.5*span),before=out.distance_m;
  out.distance_m+=segment_distance;
  auto end_at=[&](double seconds,double distance){const double end=previous_ms_+seconds*1000.;finish(i,end,distance,point(previous_,s,seconds/dt,double(previous_.tow_ms)+seconds*1000.,double(previous_.tow_ms)+dt*1000.));};
  if(def.kind==Kind::Distance && out.distance_m>=def.metres){
   const double remaining=def.metres-before,disc=std::max(0.,start_v*start_v+2*a*remaining);
   double within=std::abs(a)<1e-9?remaining/std::max(start_v,1e-9):2*remaining/(start_v+std::sqrt(disc));
   end_at(segment_start+std::clamp(within,0.,span),def.metres);continue;
  }
  if(def.kind==Kind::Acceleration && pv<def.end_mps && v>=def.end_mps){
   const double t=(def.end_mps-pv)/(v-pv)*dt;
   end_at(t,before+(start_v+def.end_mps)*.5*(t-segment_start));continue;
  }
  if(def.kind==Kind::Braking){
   if(run.stop_ms>=0&&v>.3)run.stop_ms=-1;
   const double stop_a=run.last_decel<-.5?run.last_decel:a;
   if(run.stop_ms<0&&pv>.3&&v<=.3&&stop_a<=-.5&&pv/(-stop_a)<=dt+1){
    const double t=pv/(-stop_a);run.stop_ms=previous_ms_+t*1000.;run.stop_distance=before+std::max(0.,(start_v*.5)*(t-segment_start));
    run.stop_point=point(previous_,s,t/dt,double(previous_.tow_ms)+t*1000.,double(previous_.tow_ms)+dt*1000.);
   }
   if(v>.3&&a<-.5)run.last_decel=a;
   if(run.stop_ms>=0&&v<=.3&&now-run.stop_ms>=500){finish(i,run.stop_ms,run.stop_distance,run.stop_point);continue;}
  }
  const bool abort=(def.kind==Kind::Braking?v>def.start_mps+1:(def.start_mps>0?v<def.start_mps-1:v<=.3));
  if(abort){out.phase=Phase::Invalid;out.fault=Fault::Aborted;out.sequence=++view_.revision;}
 }
 if(v>.5)stationary_=false;
 previous_=s;previous_ms_=now;
}
}
