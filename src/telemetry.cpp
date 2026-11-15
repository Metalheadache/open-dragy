// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
// Modified 2026-11-15: bounded protocol diagnostics and receiver setup update.
#include "dragy_ref/telemetry.hpp"
#include <algorithm>
#include <cstring>
namespace dragy_ref {
static uint16_t u16(const uint8_t* p) { return p[0] | (uint16_t(p[1])<<8); }
static uint32_t u32(const uint8_t* p) { return u16(p) | (uint32_t(u16(p+2))<<16); }
static int32_t i32(const uint8_t* p) { uint32_t u=u32(p); int32_t i; std::memcpy(&i,&u,4); return i; }
bool Sample::position_valid() const {
    return position_rejections()==0;
}
uint32_t Sample::position_rejections() const {
    uint32_t reasons=0;
    if(!(flags&1)||(fix!=3&&fix!=4))reasons|=RejectFix;
    if(lat_e7< -900000000||lat_e7>900000000||lon_e7< -1800000000||lon_e7>1800000000)reasons|=RejectPosition;
    if(speed_mm_s<0||speed_mm_s>150000)reasons|=RejectSpeed;
    if(!hacc_mm||hacc_mm>50000)reasons|=RejectAccuracy;
    return reasons;
}
bool Sample::utc_valid() const {
    return (valid&3)==3 && year>=2020 && year<=2099 && month>=1 && month<=12 && day>=1 && day<=31 &&
        hour<24 && minute<60 && second<=60;
}
bool decode_nav_pvt(const uint8_t* f, size_t n, Sample& s) {
    if(n!=100 || f[0]!=0xb5 || f[1]!=0x62 || f[2]!=1 || f[3]!=7 || u16(f+4)!=92) return false;
    uint8_t a=0,b=0;
    for(size_t i=2;i<98;++i) { a+=f[i]; b+=a; }
    if(a!=f[98] || b!=f[99]) return false;
    const uint8_t* p=f+6;
    s.tow_ms=u32(p); s.year=u16(p+4); s.month=p[6]; s.day=p[7]; s.hour=p[8];
    s.minute=p[9]; s.second=p[10]; s.valid=p[11]; s.time_accuracy_ns=u32(p+12); s.nano=i32(p+16);
    s.fix=p[20]; s.flags=p[21]; s.satellites=p[23]; s.lon_e7=i32(p+24); s.lat_e7=i32(p+28);
    s.height_mm=i32(p+32); s.msl_mm=i32(p+36); s.hacc_mm=u32(p+40); s.vacc_mm=u32(p+44);
    s.speed_mm_s=i32(p+60); s.heading_e5=i32(p+64); s.sacc_mm_s=u32(p+68);
    return true;
}
void Parser::drop(size_t n) { used_-=n; std::memmove(buffer_.data(),buffer_.data()+n,used_); }
void Parser::feed(const uint8_t* p, size_t n, Handler h, void* ctx, MessageHandler message) {
    for(size_t i=0;i<n;++i) {
        if(used_==buffer_.size()) { drop(1); ++discarded; }
        buffer_[used_++]=p[i];++received_;
        while(used_>=2) {
            if(buffer_[0]!=0xb5 || buffer_[1]!=0x62) { drop(1); ++discarded; continue; }
            if(used_<6) break;
            size_t len=u16(buffer_.data()+4)+8;
            if(len>buffer_.size()) { drop(1); ++discarded;++length_errors;continue; }
            if(used_<len) break;
            uint8_t a=0,b=0;
            for(size_t j=2;j<len-2;++j){a+=buffer_[j];b+=a;}
            if(a!=buffer_[len-2]||b!=buffer_[len-1]){++checksum_errors;drop(1);continue;}
            ++ubx_frames;
            if(message)message({buffer_[2],buffer_[3],buffer_.data()+6,len-8,received_-used_},ctx);
            Sample s;
            if(decode_nav_pvt(buffer_.data(),len,s)) { ++frames; if(h)h(s,buffer_.data(),ctx); }
            else ++other_frames;
            // A checksum-valid unsupported payload is opaque. Do not mistake a
            // nested B5 62 sequence inside it for a separate GPS solution.
            drop(len);
        }
    }
}
Epoch Timebase::accept(uint32_t tow) {
    delta_ms=0;
    if(tow>=gps_week_ms) return Epoch::Invalid;
    if(!initial_) { initial_=true; previous_=tow; return Epoch::First; }
    if(tow==previous_) return Epoch::Duplicate;
    int64_t dt=int64_t(tow)-previous_;
    if(previous_>gps_week_ms-60000 && tow<60000) dt+=gps_week_ms;
    if(dt<0 && dt>=-2000) return Epoch::Backward;
    if(dt<0) { previous_=tow; elapsed_ms=0; return Epoch::Reset; }
    previous_=tow; delta_ms=uint32_t(dt); elapsed_ms+=delta_ms; return Epoch::Forward;
}
void Statistics::accept(const Sample& s, Epoch e, uint32_t dt) {
    if(e==Epoch::Duplicate || e==Epoch::Backward) return;
    bool good=s.position_valid() && s.sacc_mm_s>0 && s.sacc_mm_s<=3000;
    double speed=s.speed_mm_s/1000.0;
    if(good) peak_kmh=std::max(peak_kmh,speed*3.6);
    if(good && previous_valid_ && e==Epoch::Forward && dt>0 && dt<=500) {
        // Suppress stationary wander only for trip integration, never the raw sample.
        if(speed>=0.5 && previous_mps_>=0.5) {
            distance_m+=(speed+previous_mps_)*0.5*dt/1000.0;
            moving_s+=dt/1000.0;
        }
    }
    previous_valid_=good && e!=Epoch::Invalid; previous_mps_=speed;
}
uint32_t crc32(const uint8_t* p,size_t n) {
    uint32_t c=0xffffffff;
    while(n--) { c^=*p++; for(int i=0;i<8;++i) c=(c>>1)^(0xedb88320u & (0u-(c&1))); }
    return ~c;
}
}
