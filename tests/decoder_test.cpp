// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/telemetry.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <vector>
using namespace dragy_ref;
static void put32(uint8_t* p,uint32_t n){for(int i=0;i<4;i++)p[i]=n>>(8*i);}
static void checksum(std::array<uint8_t,100>& b){uint8_t a=0,c=0;for(int i=2;i<98;i++){a+=b[i];c+=a;}b[98]=a;b[99]=c;}
static auto frame(uint32_t tow=1000){
    std::array<uint8_t,100>b{};b[0]=0xb5;b[1]=0x62;b[2]=1;b[3]=7;b[4]=92;
    auto*p=b.data()+6;put32(p,tow);p[4]=0xea;p[5]=7;p[6]=10;p[7]=15;p[8]=12;p[11]=3;
    p[20]=3;p[21]=1;p[23]=18;put32(p+24,1038000000);put32(p+28,13000000);
    put32(p+36,uint32_t(-12000));put32(p+40,800);put32(p+44,1500);put32(p+60,10000);put32(p+68,100);
    checksum(b);return b;
}
static int seen;
static Sample last;
static void receive(const Sample&s,const uint8_t*,void*){seen++;last=s;}
int main(){
    auto b=frame();Sample s;
    assert(decode_nav_pvt(b.data(),b.size(),s));assert(s.position_valid());assert(s.utc_valid());
    assert(s.lon_e7==1038000000 && s.lat_e7==13000000 && s.msl_mm==-12000 && s.speed_mm_s==10000);
    for(size_t cut=0;cut<=100;cut++){
        Parser p;seen=0;p.feed(b.data(),cut,receive,nullptr);p.feed(b.data()+cut,100-cut,receive,nullptr);assert(seen==1);
    }
    {Parser p;seen=0;for(auto c:b)p.feed(&c,1,receive,nullptr);assert(seen==1);}
    {Parser p;seen=0;std::vector<uint8_t> v(10000,0xff);v.insert(v.end(),b.begin(),b.end());v.insert(v.end(),b.begin(),b.end());p.feed(v.data(),v.size(),receive,nullptr);assert(seen==2);}
    {Parser p;seen=0;auto bad=b;bad[30]^=1;p.feed(bad.data(),bad.size(),receive,nullptr);p.feed(b.data(),b.size(),receive,nullptr);assert(seen==1 && p.checksum_errors==1);}
    {Parser p;seen=0;uint8_t malicious[]={0xb5,0x62,1,7,0xff,0xff};p.feed(malicious,6,receive,nullptr);p.feed(b.data(),b.size(),receive,nullptr);assert(seen==1);}
    {Parser p;seen=0;p.feed(b.data(),50,receive,nullptr);p.reset();p.feed(b.data(),100,receive,nullptr);assert(seen==1);}
    assert(!decode_nav_pvt(b.data(),99,s));
    s.flags=0;assert(!s.position_valid());s.flags=1;s.fix=2;assert(!s.position_valid());s.fix=3;
    s.speed_mm_s=-1;assert(!s.position_valid());s.speed_mm_s=10000;s.lat_e7=900000001;assert(!s.position_valid());
    s=last;s.valid=0;assert(!s.utc_valid());
    Timebase t;assert(t.accept(1000)==Epoch::First);assert(t.accept(1040)==Epoch::Forward && t.delta_ms==40);
    assert(t.accept(1040)==Epoch::Duplicate);assert(t.accept(1020)==Epoch::Backward);assert(t.accept(1080)==Epoch::Forward && t.delta_ms==40);
    assert(t.accept(gps_week_ms)==Epoch::Invalid);assert(t.accept(1120)==Epoch::Forward && t.delta_ms==40);
    t.reset();assert(t.accept(gps_week_ms-20)==Epoch::First);assert(t.accept(20)==Epoch::Forward && t.delta_ms==40);
    t.reset();assert(t.accept(500000)==Epoch::First);assert(t.accept(10)==Epoch::Reset);assert(t.accept(50)==Epoch::Forward && t.delta_ms==40);
    Statistics stats;decode_nav_pvt(b.data(),100,s);stats.accept(s,Epoch::First,0);stats.accept(s,Epoch::Forward,100);
    assert(fabs(stats.distance_m-1)<1e-9);stats.accept(s,Epoch::Duplicate,0);assert(fabs(stats.distance_m-1)<1e-9);
    stats.accept(s,Epoch::Forward,2000);assert(fabs(stats.distance_m-1)<1e-9);
    s.flags=0;stats.accept(s,Epoch::Forward,100);s.flags=1;stats.accept(s,Epoch::Forward,100);assert(fabs(stats.distance_m-1)<1e-9);
    stats.break_continuity();stats.accept(s,Epoch::Forward,100);assert(fabs(stats.distance_m-1)<1e-9);
    assert(crc32((const uint8_t*)"123456789",9)==0xcbf43926);
    puts("PASS: all 101 fragment splits, bytewise/batched/noisy/corrupt streams, bounds, signed fields, validity, epochs, rollover, reset, gap exclusion, CRC32");
}
