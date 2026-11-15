// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "dragy_ref/receiver.hpp"
#include "../tools/synthetic.hpp"
#include <cassert>
#include <cstdio>
using namespace dragy_ref;
struct Harness {
    Ingress<32> q;BoundaryControl gate,watch;ReceiverDecoder<Ingress<32>> decoder;
    uint64_t now{10000000};uint32_t tow{100000};
    Harness():decoder(q,gate,watch){q.new_session();gate.publish(q.boundary(),false);decoder.poll(now);decoder.timing.arm();}
    void frame(bool drain=true,unsigned dt=40){now+=dt*1000;tow+=dt;auto f=synthetic::frame(tow);assert(q.push(q.version().session,f.data(),f.size(),now));if(drain)decoder.poll(now);}
    void warm(){for(unsigned i=0;i<10;++i)frame();assert(decoder.timing.ready(now));}
};
int main(){
    {Harness h;for(unsigned i=0;i<12;++i)h.frame(false);h.gate.publish(h.q.boundary(),false);h.decoder.poll(h.now);assert(!h.decoder.timing.ready(h.now));
     for(unsigned i=0;i<8;++i)h.frame();assert(h.decoder.timing.ready(h.now));}
    {Harness h;h.gate.publish(h.q.boundary(),true);for(unsigned i=0;i<12;++i)h.frame();assert(h.decoder.timing.clean_intervals()>=8&&!h.decoder.timing.ready(h.now));
     h.gate.publish(h.q.boundary(),false);h.decoder.poll(h.now);for(unsigned i=0;i<8;++i)h.frame();assert(!h.decoder.timing.ready(h.now));h.frame();assert(h.decoder.timing.ready(h.now));}
    {Harness h;h.warm();uint8_t bad[513]{};assert(!h.q.push(h.q.version().session,bad,sizeof(bad),h.now));h.decoder.poll(h.now);assert(!h.decoder.timing.ready(h.now));
     for(unsigned i=0;i<9;++i)h.frame();assert(h.decoder.timing.ready(h.now));}
    {Harness h;h.warm();h.q.new_session();h.gate.publish(h.q.boundary(),false);h.decoder.poll(h.now);assert(!h.decoder.timing.ready(h.now));for(unsigned i=0;i<9;++i)h.frame();assert(h.decoder.timing.ready(h.now));}
    {Harness h;h.warm();h.frame(true,80);assert(!h.decoder.timing.ready(h.now));h.frame();assert(h.decoder.timing.ready(h.now)&&h.decoder.timing.bridged==1);
     h.decoder.poll(h.now+600000);assert(!h.decoder.timing.ready(h.now+600000));}
    {Harness h;for(unsigned i=0;i<30;++i)h.frame(true,100);assert(!h.decoder.timing.ready(h.now));assert(h.decoder.parser().frames==30);}
    // A paused live consumer must not publish a finish from stale queued data.
    {Harness h;
     for(unsigned ms=0;ms<=9200;ms+=40){auto f=synthetic::frame(100000+ms,std::max(0,int(ms)-2000)*4);
        assert(h.q.push(h.q.version().session,f.data(),f.size(),10000000+ms*1000ull));
        if(ms<=8800)h.decoder.poll(10000000+ms*1000ull);
     }
     h.decoder.poll(10000000+9800*1000ull);
     assert(h.decoder.timing.view().channels[0].result.phase!=drag::Phase::Complete);
     assert(!h.decoder.timing.ready(10000000+9800*1000ull));}
    // Stale complete/partial ACKs are tested through the same consumer as ESP.
    const uint8_t ack[]={0xb5,0x62,5,1,2,0,6,0x8a,0x98,0xc1};
    for(unsigned cut=0;cut<=sizeof(ack);++cut){Harness h;
        if(cut)assert(h.q.push(h.q.version().session,ack,cut,h.now));h.decoder.poll(h.now);
        h.watch.publish(h.q.boundary(),5);
        if(cut<sizeof(ack))assert(h.q.push(h.q.version().session,ack+cut,sizeof(ack)-cut,h.now));h.decoder.poll(h.now);
        assert(h.decoder.reply()==(cut==0?ble::Reply::Ack:ble::Reply::None));
    }
    // A corrupt auxiliary UBX resets startup evidence but not a running window.
    {Harness h;h.warm();auto bad=std::array<uint8_t,10>{0xb5,0x62,5,1,2,0,6,0x8a,0x98,0};
     assert(h.q.push(h.q.version().session,bad.data(),bad.size(),h.now));h.decoder.poll(h.now);h.frame();
     assert(h.decoder.parser().checksum_errors==1&&h.decoder.timing.ready(h.now));}
    puts("PASS actual receiver consumer: setup boundaries, queued input exclusion, pending observation, overflow recovery, sessions, strict10 rejection, confirmed gaps, stale timeout and partial ACK attribution");
}
