// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "dragy_ref/rate_setup.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace dragy_ref::ble;
struct IO {
    uint32_t serial{},active{},done{},watch_token{};unsigned models{},writes{},firmwares{},profiles{};
    Receiver actual{Receiver::Pro};Completion result{Completion::Success};Reply observed{Reply::None};
    bool start_ok{true},can_write{true},clean{},ready{},pending{},request_start_ok{true},firmware_start_ok{true};
    uint32_t next_token(){active=++serial;return active;}
    void invalidate(){active=++serial;}
    void gate(bool p){pending=p;}
    void watch(uint32_t token){watch_token=token;}
    bool model(uint32_t){++models;return start_ok;}
    bool firmware(uint32_t){++firmwares;return firmware_start_ok;}
    bool write(uint32_t,const std::array<uint8_t,18>& b){++writes;assert(b==pro_rate_frame());return request_start_ok;}
    Completion completion(uint32_t token){return done==token?result:Completion::Pending;}
    Receiver model_receiver(){return actual;}
    bool writable(){return can_write;}
    bool clean25(uint64_t){return clean;}
    bool timing_ready(uint64_t){return ready;}
    Reply reply(uint32_t token){return token==watch_token?observed:Reply::None;}
    void log(const char*){}
    void profile(Receiver,bool){++profiles;}
    void complete(Completion c=Completion::Success){result=c;done=active;}
};
int main(){
    // All named profiles; legacy actual identity takes precedence over hints.
    for(auto model:{Receiver::Pro,Receiver::Drg70,Receiver::Lite,Receiver::Legacy,Receiver::Unknown})
    for(auto hint:{Receiver::Unknown,Receiver::Drg70,Receiver::Lite})
    for(bool provisional:{false,true})for(bool enabled:{false,true}){
        IO io;io.actual=model;RateSetup<IO> setup(io);setup.start(0,hint,enabled,provisional);
        io.complete();setup.poll(1);auto selected=rate_receiver(model,hint);
        const bool expected=enabled&&request_25hz(selected)&&(selected==Receiver::Pro||provisional);
        assert((setup.stage()==RateSetup<IO>::Stage::Observe)==expected);
        setup.poll(750001);assert(io.writes==unsigned(expected));
    }
    {IO io;io.clean=true;RateSetup<IO> s(io);s.start(0,Receiver::Unknown,true);io.complete();s.poll(1);s.poll(750000);assert(io.writes==0&&io.pending);
     s.poll(750001);assert(io.writes==0&&!io.pending&&io.firmwares==1);}
    for(auto result:{Completion::Success,Completion::Failure}){
        IO io;RateSetup<IO> s(io);s.start(0,Receiver::Unknown,true);io.complete();s.poll(1);s.poll(750001);
        assert(io.writes==1&&io.watch_token&&io.pending);io.complete(result);s.poll(750002);
        assert(!io.pending&&io.firmwares==1&&s.watching()==(result==Completion::Success));
        io.complete();s.poll(750003);io.observed=Reply::Ack;s.poll(800000);assert(s.watching()==(result==Completion::Success)); // ACK alone is not 25 Hz
        io.ready=true;s.poll(900000);assert(!s.watching());
    }
    {IO io;RateSetup<IO> s(io);s.start(0,Receiver::Unknown,true);io.complete();s.poll(1);s.poll(750001);io.complete();s.poll(750002);io.observed=Reply::Nak;s.poll(800000);assert(!s.watching()&&io.watch_token==0);}
    {IO io;RateSetup<IO> s(io);s.start(0,Receiver::Unknown,true);s.poll(3000000);assert(io.writes==0&&io.firmwares==0&&!io.pending);
     io.complete();s.poll(4000000);assert(io.writes==0&&io.firmwares==0);
     s.start(4500000,Receiver::Unknown,true,false,false);assert(io.models==1&&!io.pending); // late completion cannot overlap
     s.start(5000000,Receiver::Unknown,true);io.complete();s.poll(5000001);s.poll(5750001);assert(io.writes==1);}
    {IO io;RateSetup<IO> s(io);s.start(0,Receiver::Unknown,true);io.complete();s.poll(1);s.poll(750001);s.poll(3750001);assert(io.firmwares==0&&!s.watching()&&!io.pending);}
    {IO io;io.start_ok=false;RateSetup<IO> s(io);s.start(0,Receiver::Unknown,true);assert(!io.pending&&io.writes==0&&io.firmwares==1);}
    {IO io;io.can_write=false;RateSetup<IO> s(io);s.start(0,Receiver::Unknown,true);io.complete();s.poll(1);assert(io.writes==0&&io.firmwares==1);}
    {IO io;io.request_start_ok=false;RateSetup<IO> s(io);s.start(0,Receiver::Unknown,true);io.complete();s.poll(1);s.poll(750001);assert(!io.pending&&io.firmwares==1&&io.watch_token==0);}
    {IO io;RateSetup<IO> s(io);s.start(0,Receiver::Unknown,true);io.complete();s.poll(1);s.poll(750001);io.complete();s.poll(750002);s.poll(3750002);assert(io.firmwares==1&&s.watching());s.poll(10750001);assert(!s.watching());}
    puts("PASS rate controller: 60 profile/opt-in combinations, existing25 skip, 10Hz request, ACK/NAK independence, failures, timeouts, stale completions, reconnect and no overlapping ATT procedures");
}
