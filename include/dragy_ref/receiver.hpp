// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#pragma once
#include "dragy_ref/ingress.hpp"
#include "dragy_ref/timing.hpp"
#include "dragy_ref/protocol.hpp"
namespace dragy_ref {
// Shared consumer integration for the ESP example and host fault tests. The
// connection task owns gate/reply controls; the serialized BLE callback owns
// queue.push(). Only the consumer may poll this object or arm its detectors.
template<class Queue> class ReceiverDecoder {
public:
    using OnSample=void(*)(const Sample&,uint64_t,void*);
    ReceiverDecoder(Queue& queue,BoundaryControl& gate,BoundaryControl& reply,OnSample hook=nullptr,void* ctx=nullptr)
        :gate_control_(gate),reply_control_(reply),decoder_(queue,sample,gap,this,fragment,message),hook_(hook),context_(ctx){}
    void poll(uint64_t now){now_=now;sync();timing.poll(now);decoder_.drain();timing.poll(now);}
    uint32_t gate_revision()const{return gate_.revision;}
    uint32_t request()const{return watch_.value;}
    uint32_t last_delta()const{return delta_;}
    uint64_t last_sample_us()const{return last_sample_;}
    ble::Reply reply()const{return replies.reply(watch_.value);}
    const Parser& parser()const{return decoder_.parser();}
    drag::TimingGate25 timing;
    ble::ReplyObserver replies;
    ble::QualityCounters quality;
private:
    BoundaryControl& gate_control_;BoundaryControl& reply_control_;
    Decoder<Queue> decoder_;OnSample hook_;void* context_;
    BoundaryControl::View gate_{},watch_{};Boundary delivery_{};
    uint64_t fragment_offset_{},last_sample_{},now_{};uint32_t delta_{};Timebase time_;
    bool sync(){
        BoundaryControl::View v;
        const bool valid=gate_control_.read(v);
        if(!valid)timing.interrupt();
        else if(v.revision!=gate_.revision){gate_=v;timing.configure(v.value!=0);}
        if(!reply_control_.read(v))replies.reset();
        else if(v.revision!=watch_.revision){watch_=v;replies.arm(v.value,v.boundary);}
        return valid;
    }
    static void fragment(Boundary b,uint64_t,uint64_t offset,void* p){
        auto& d=*static_cast<ReceiverDecoder*>(p);d.delivery_=b;d.fragment_offset_=offset;
        d.sync();d.replies.fragment(b,offset);
    }
    static void message(const Parser::Message& m,void* p){
        auto& d=*static_cast<ReceiverDecoder*>(p);d.sync();d.replies.fragment(d.delivery_,d.fragment_offset_);d.replies.observe(m);
    }
    static void sample(const Sample& s,uint64_t arrival,void* p){
        auto& d=*static_cast<ReceiverDecoder*>(p);const bool valid=d.sync();d.last_sample_=arrival;d.quality.observe(s);
        if(d.time_.accept(s.tow_ms)==Epoch::Forward)d.delta_=d.time_.delta_ms;
        const auto boundary=d.gate_.boundary;
        const bool fresh=arrival>=d.now_||d.now_-arrival<=250000;
        const bool after=valid&&fresh&&d.delivery_.session==boundary.session&&int32_t(d.delivery_.sequence-boundary.sequence)>0;
        d.timing.accept(s,arrival,d.parser().checksum_errors+d.parser().length_errors,after);
        if(d.hook_)d.hook_(s,arrival,d.context_);
    }
    static void gap(void* p){
        auto& d=*static_cast<ReceiverDecoder*>(p);d.timing.interrupt();d.replies.reset();d.time_.reset();d.delta_=0;d.last_sample_=0;
    }
};
}
