// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "dragy_ref/ingress.hpp"
#include "dragy_ref/protocol.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
using namespace dragy_ref;
static std::vector<uint8_t> ack(uint8_t id=1,uint8_t cls=6,uint8_t command=0x8a){
    std::vector<uint8_t> b{0xb5,0x62,5,id,2,0,cls,command};uint8_t a=0,c=0;
    for(size_t i=2;i<b.size();++i){a+=b[i];c+=a;}b.push_back(a);b.push_back(c);return b;
}
struct Harness {
    Ingress<8> q;ble::ReplyObserver reply;Decoder<Ingress<8>> decoder;
    Harness():decoder(q,sample,gap,this,fragment,message){q.new_session();}
    static void sample(const Sample&,uint64_t,void*){}
    static void gap(void* p){static_cast<Harness*>(p)->reply.reset();}
    static void fragment(Boundary b,uint64_t,uint64_t offset,void* p){static_cast<Harness*>(p)->reply.fragment(b,offset);}
    static void message(const Parser::Message& m,void* p){static_cast<Harness*>(p)->reply.observe(m);}
    void push(const std::vector<uint8_t>& b,size_t a,size_t n){assert(q.push(q.version().session,b.data()+a,n,1000000));}
};
int main(){
    auto b=ack();
    for(size_t cut=0;cut<=b.size();++cut){
        Harness h;h.decoder.drain();h.push(b,0,cut);h.decoder.drain();
        h.reply.arm(1,h.q.boundary());h.push(b,cut,b.size()-cut);h.decoder.drain();
        assert(h.reply.reply(1)==(cut==0?ble::Reply::Ack:ble::Reply::None));
        h.push(b,0,b.size());h.decoder.drain();assert(h.reply.reply(1)==ble::Reply::Ack);
        auto nak=ack(0);h.push(nak,0,nak.size());h.decoder.drain();assert(h.reply.reply(1)==ble::Reply::Ack);
    }
    {Harness h;h.decoder.drain();h.push(b,0,b.size());h.reply.arm(2,h.q.boundary());h.decoder.drain();assert(h.reply.reply(2)==ble::Reply::None);}
    {Harness h;h.decoder.drain();h.reply.arm(3,h.q.boundary());auto wrong=ack(1,6,1);h.push(wrong,0,wrong.size());h.decoder.drain();assert(h.reply.reply(3)==ble::Reply::None);
     auto nak=ack(0);h.push(nak,0,nak.size());h.decoder.drain();assert(h.reply.reply(3)==ble::Reply::Nak);
     h.push(b,0,b.size());h.decoder.drain();assert(h.reply.reply(3)==ble::Reply::Nak);
     h.q.new_session();h.push(b,0,b.size());h.decoder.drain();assert(h.reply.reply(3)==ble::Reply::None);}
    {Harness h;h.decoder.drain();h.reply.arm(4,h.q.boundary());auto bad=b;bad.back()^=1;h.push(bad,0,bad.size());h.decoder.drain();assert(h.reply.reply(4)==ble::Reply::None);}
    {ble::ReplyObserver o;o.arm(5,{1,0,UINT32_MAX});o.fragment({1,0,0},0);o.observe({5,1,b.data()+6,2,0});assert(o.reply(5)==ble::Reply::Ack);
     o.arm(6,{1,0,2});o.fragment({1,1,3},0);o.observe({5,1,b.data()+6,2,0});assert(o.reply(6)==ble::Reply::None);}
    BoundaryControl mailbox;std::atomic<bool> done{false};
    std::thread writer([&]{for(uint32_t i=1;i<100000;++i)mailbox.publish({i,i+1,i+2},i+3);done=true;});
    unsigned coherent=0;
    while(!done){BoundaryControl::View v;if(mailbox.read(v)&&v.revision){assert(v.boundary.loss==v.boundary.session+1&&v.boundary.sequence==v.boundary.session+2&&v.value==v.boundary.session+3);++coherent;}}
    writer.join();BoundaryControl::View final;assert(mailbox.read(final)&&final.value==100002);(void)coherent;
    puts("PASS reply fragmentation, queued/partial stale replies, command matching, first reply, reset/loss/wrap boundaries and concurrent coherent control snapshots");
}
