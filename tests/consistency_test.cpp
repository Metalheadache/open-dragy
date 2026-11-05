// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "dragy_ref/ingress.hpp"
#include "dragy_ref/rate.hpp"
#include "../tools/synthetic.hpp"
#include <cassert>
#include <algorithm>
#include <thread>
#include <cstdio>
using Queue=dragy_ref::Ingress<4>;
struct State {unsigned frames{},gaps{};};
static void sample(const dragy_ref::Sample& s,uint64_t,void* p){assert(s.speed_mm_s==10000);++static_cast<State*>(p)->frames;}
static void gap(void* p){++static_cast<State*>(p)->gaps;}
int main() {
    Queue q;State state;dragy_ref::Decoder<Queue> decoder(q,sample,gap,&state);
    auto token=q.new_session();auto b=synthetic::frame(100000);
    q.push(token,b.data(),50,1000000);decoder.drain();assert(state.frames==0);
    auto old=token;token=q.new_session();assert(!q.push(old,b.data()+50,50,1000001));
    q.push(token,b.data(),100,1000002);decoder.drain();assert(state.frames==1&&state.gaps==2);
    q.push(token,b.data(),50,1000003);decoder.drain();
    for(int i=0;i<3;++i)assert(q.push(token,b.data(),10,1000004));
    assert(!q.push(token,b.data(),10,1000004));decoder.drain();
    q.push(token,b.data()+50,50,1000005);decoder.drain();assert(state.frames==1);
    q.push(token,b.data(),100,1000006);decoder.drain();assert(state.frames==2&&q.drops()==1);
    assert(!q.push(token,b.data(),513,1000007));decoder.drain();assert(q.drops()==2);
    // Queued old-session fragments are discarded too, not just late callbacks.
    q.push(token,b.data(),100,1000008);token=q.new_session();decoder.drain();assert(state.frames==2);
    dragy_ref::ble::CadenceVerifier rate;
    for(unsigned i=0;i<25;++i)assert(!rate.observe(100000+i*40));
    assert(rate.observe(101000));assert(!rate.observe(101080));
    for(unsigned i=1;i<=25;++i)rate.observe(101080+i*40);
    assert(rate.confirmed());rate.reset();assert(!rate.confirmed());
    assert(!rate.observe(dragy_ref::gps_week_ms));
    for(unsigned i=0;i<=25;++i)rate.observe((dragy_ref::gps_week_ms-400+i*40)%dragy_ref::gps_week_ms);
    assert(rate.confirmed());
    // Genuine two-thread SPSC handoff; producer retries only in this stress
    // test. The BLE callback must always drop immediately instead of retrying.
    dragy_ref::Ingress<64> threaded;auto session=threaded.new_session();constexpr unsigned count=20000;
    std::thread producer([&]{for(unsigned i=0;i<count;++i){while(!threaded.push(session,b.data(),100,i))std::this_thread::yield();}});
    dragy_ref::Ingress<64>::Fragment f;unsigned got=0;
    while(got<count){if(threaded.pop(f)){assert(f.size==100&&f.arrival_us==got&&std::equal(b.begin(),b.end(),f.bytes.begin()));++got;}else std::this_thread::yield();}
    producer.join();puts("PASS consistency: fragment/session isolation, loss reset, recovery, strict cadence, rollover, two-thread SPSC order");
}
