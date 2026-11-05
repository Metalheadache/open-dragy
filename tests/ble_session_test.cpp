// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/ble_session.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace dragy_ref;
struct Gatt {
 int fault=-1;uint8_t a=0x90,b=0;uint16_t enabled=1;std::string error;std::vector<ble::Stage> stages;
 bool discovered=false,subscribed=false;
 void stage(ble::Stage s){stages.push_back(s);}
 bool fail(const char* why){error=why;return false;}
 bool discover(){discovered=true;return fault!=0;}
 bool has_cccd(){assert(discovered);return fault!=1;}
 bool read_challenge(std::array<uint8_t,2>& out){out={a,b};return fault!=2;}
 bool write_response(const std::array<uint8_t,4>& v){assert(v[0]==a&&v[1]==b&&v[2]==(a^b)&&v[3]==(a&b));return fault!=3;}
 bool subscribe(){assert(discovered);subscribed=fault!=4;return subscribed;}
 bool read_cccd(uint16_t& v){assert(subscribed);v=enabled;return fault!=5;}
};
int main(){
 for(unsigned a=0;a<256;a++)for(unsigned b=0;b<256;b++){Gatt io;io.a=a;io.b=b;assert(ble::setup(io));assert(io.error.empty());assert(io.stages.size()==5);}
 const char* failures[]={"GATT discovery failed","FD02 CCCD missing","FD03 read failed","FD03 write failed","FD02 subscribe failed","FD02 CCCD read failed"};
 for(int f=0;f<6;f++){Gatt io;io.fault=f;assert(!ble::setup(io));assert(io.error==failures[f]);}
 for(uint16_t bits:{0,2,3,0x100}){Gatt io;io.enabled=bits;assert(!ble::setup(io));assert(io.error=="FD02 CCCD not enabled");}
 using S=ble::Stream;
 assert(ble::stream(5000000,1000000,0,0)==S::Waiting);
 assert(ble::stream(11000000,1000000,0,0)==S::NoNotifications);
 assert(ble::stream(11000000,1000000,10900000,0)==S::Undecodable);
 assert(ble::stream(11000000,1000000,10900000,10900000)==S::Live);
 assert(ble::stream(11000000,1000000,10900000,1000000)==S::Stale);
 assert(ble::stream(22000000,1000000,1000000,1000000)==S::NoNotifications);
 assert(ble::stream(0,1000000,0,0)==S::Waiting);
 for(unsigned i=0;i<100;i++)assert(ble::backoff(i)==(i==0?1:i==1?2:i==2?5:10));
 // The warning is separate from the initial recovery deadline. Fresh no-fix
 // frames have no position-acquisition deadline; established silence does.
 assert(!ble::reconnect_due(11000000,1000000,0,0,false));
 assert(!ble::reconnect_due(180999999,1000000,0,0,false));
 assert(ble::reconnect_due(181000000,1000000,0,0,false));
 assert(!ble::repair_due(30999999,1000000,false,false));
 assert(ble::repair_due(31000000,1000000,false,false));
 assert(!ble::repair_due(31000000,1000000,true,false));
 assert(!ble::repair_due(31000000,1000000,false,true));
 assert(!ble::reconnect_due(1800000000,1000000,1799900000,1799900000,true));
 assert(!ble::reconnect_due(10999999,1000000,1000000,1000000,true));
 assert(ble::reconnect_due(11000000,1000000,1000000,1000000,true));
 assert(!ble::reconnect_due(0,1000000,0,0,false));
 puts("PASS: BLE setup controller: 65,536 challenge pairs; discovery/CCCD/read/write/subscribe/readback failures; no-packet/undecodable/stale streams; bounded retry");
}
