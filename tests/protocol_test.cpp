// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/telemetry.hpp"
#include "../tools/synthetic.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace dragy_ref;
static std::vector<uint8_t> message(uint8_t cls,uint8_t id,const std::vector<uint8_t>& payload){
    std::vector<uint8_t> b{0xb5,0x62,cls,id,uint8_t(payload.size()),uint8_t(payload.size()>>8)};
    b.insert(b.end(),payload.begin(),payload.end());uint8_t a=0,c=0;
    for(size_t i=2;i<b.size();++i){a+=b[i];c+=a;}b.push_back(a);b.push_back(c);return b;
}
struct Seen {unsigned samples{};std::vector<unsigned> keys;std::vector<uint64_t> offsets;};
static void sample(const Sample&,const uint8_t*,void* ctx){++static_cast<Seen*>(ctx)->samples;}
static void observe(const Parser::Message& m,void* ctx){
    auto& s=*static_cast<Seen*>(ctx);s.keys.push_back((unsigned(m.cls)<<8)|m.id);s.offsets.push_back(m.offset);
    if(m.cls==5&&m.id==1&&m.size==2)assert(m.payload[0]==6&&m.payload[1]==0x8a);
}
int main(){
    auto pvt=message(1,7,std::vector<uint8_t>(92));
    auto ack=message(5,1,{6,0x8a}),nak=message(5,0,{6,0x8a});
    auto dop=message(1,4,std::vector<uint8_t>(18));
    // An entire, checksum-valid NAV-PVT nested in an unknown payload must be opaque.
    auto nested=message(0x99,0x42,pvt);
    auto maximum=message(0x99,0x43,std::vector<uint8_t>(512,0xa5));
    auto zero=message(0x99,0x44,{});
    std::vector<uint8_t> stream;std::vector<uint64_t> offsets;
    for(const auto& b:{ack,nested,dop,maximum,zero,nak,pvt}){
        offsets.push_back(stream.size());stream.insert(stream.end(),b.begin(),b.end());
    }
    for(size_t cut=0;cut<=stream.size();++cut){
        Parser p;Seen s;p.feed(stream.data(),cut,sample,&s,observe);
        p.feed(stream.data()+cut,stream.size()-cut,sample,&s,observe);
        assert(s.samples==1&&p.frames==1&&p.ubx_frames==7&&p.other_frames==6);
        assert(p.checksum_errors==0&&p.discarded==0&&s.offsets==offsets);
        assert((s.keys==std::vector<unsigned>{0x0501,0x9942,0x0104,0x9943,0x9944,0x0500,0x0107}));
    }
    {Parser p;Seen s;for(auto b:stream)p.feed(&b,1,sample,&s,observe);assert(s.samples==1&&p.ubx_frames==7);}
    {Parser p;Seen s;auto bad=ack;bad.back()^=1;p.feed(bad.data(),bad.size(),sample,&s,observe);
     p.feed(pvt.data(),pvt.size(),sample,&s,observe);assert(p.checksum_errors==1&&s.samples==1&&s.keys.size()==1);}
    // Startup capture shape: a 24-satellite NAV-SAT is truncated by 13 bytes,
    // followed by valid ACKs. Reject it strictly and recover without losing PVT.
    // Synthetic payload only; no private receiver coordinates or satellite data.
    {
        std::vector<uint8_t> payload(296);payload[4]=1;payload[5]=24;
        auto partial=message(1,0x35,payload);partial.resize(partial.size()-13);
        std::vector<uint8_t> burst;
        for(const auto& b:{pvt,pvt,partial,ack,ack,pvt,pvt})burst.insert(burst.end(),b.begin(),b.end());
        for(size_t cut=0;cut<=burst.size();++cut){
            Parser p;Seen s;p.feed(burst.data(),cut,sample,&s,observe);
            p.feed(burst.data()+cut,burst.size()-cut,sample,&s,observe);
            assert(p.checksum_errors==1&&p.length_errors==0&&s.samples==4&&p.ubx_frames==6);
            assert((s.keys==std::vector<unsigned>{0x0107,0x0107,0x0501,0x0501,0x0107,0x0107}));
            assert((s.offsets==std::vector<uint64_t>{0,100,491,501,511,611}));
        }
        Parser p;Seen s;for(auto b:burst)p.feed(&b,1,sample,&s,observe);
        assert(p.checksum_errors==1&&s.samples==4&&p.ubx_frames==6);
    }
    {Parser p;Seen s;uint8_t huge[]={0xb5,0x62,0x99,1,1,2};
     p.feed(huge,sizeof(huge),sample,&s,observe);p.feed(ack.data(),ack.size(),sample,&s,observe);
     assert(p.length_errors==1&&p.ubx_frames==1&&s.samples==0);}
    {Parser p;Seen s;p.feed(ack.data(),5,sample,&s,observe);assert(p.received()==5);p.reset();
     p.feed(ack.data()+5,ack.size()-5,sample,&s,observe);assert(s.keys.empty());
     p.feed(pvt.data(),pvt.size(),sample,&s,observe);assert(s.samples==1&&s.offsets[0]==5);}
    // Malformed NAV-PVT length is visible as other UBX, never a synthetic fix.
    {Parser p;Seen s;auto short_pvt=message(1,7,std::vector<uint8_t>(91));
     p.feed(short_pvt.data(),short_pvt.size(),sample,&s,observe);assert(s.samples==0&&p.other_frames==1);}
    Sample s;s.fix=3;s.flags=1;s.hacc_mm=1000;s.speed_mm_s=50000;assert(s.position_rejections()==0);
    s.speed_mm_s=INT32_MAX;assert(s.position_rejections()==RejectSpeed);
    s.flags=0;s.lat_e7=900000001;s.hacc_mm=0;
    assert(s.position_rejections()==(RejectFix|RejectPosition|RejectSpeed|RejectAccuracy));
    for(auto fix:{0,1,2,3,4,5})for(auto flags:{0,1,2,3})
    for(auto lat:{-900000001,-900000000,0,900000000,900000001})
    for(auto lon:{-1800000001,-1800000000,0,1800000000,1800000001})
    for(auto speed:{-1,0,46328,150000,150001,INT32_MAX})
    for(auto hacc:{0,1,50000,50001}){
        s.fix=uint8_t(fix);s.flags=uint8_t(flags);s.lat_e7=lat;s.lon_e7=lon;s.speed_mm_s=speed;s.hacc_mm=uint32_t(hacc);
        bool previous=(flags&1)&&(fix==3||fix==4)&&lat>=-900000000&&lat<=900000000&&lon>=-1800000000&&lon<=1800000000&&speed>=0&&speed<=150000&&hacc>0&&hacc<=50000;
        assert(s.position_valid()==previous);
    }
    for(int32_t speed:{32767,32768,46328,46329,65535,65536,83333,150000,150001,INT32_MAX,-1}){
        auto frame=synthetic::frame(100000,speed);
        Sample decoded;assert(decode_nav_pvt(frame.data(),frame.size(),decoded));
        assert(decoded.speed_mm_s==speed&&decoded.position_valid()==(speed>=0&&speed<=150000));
    }
    puts("PASS bounded UBX dispatcher: every split, bytewise, nested PVT exclusion, 512-byte payload, zero/oversize/corrupt/truncated NAV-SAT frames, reset boundaries and unchanged quality predicates");
}
