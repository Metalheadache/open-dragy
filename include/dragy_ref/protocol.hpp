// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#pragma once
#include "dragy_ref/control.hpp"
#include "dragy_ref/telemetry.hpp"
namespace dragy_ref::ble {
enum class Reply:uint8_t {None,Ack,Nak};
// Consumer-owned. Only a complete matching reply whose first byte follows the
// request's notification boundary may answer it. UBX ACK has no transaction ID;
// this is temporal attribution, not proof of which command caused the reply.
class ReplyObserver {
public:
    void arm(uint32_t token,Boundary after){token_=token;after_=after;result_=Reply::None;boundary_set_=false;eligible_=false;}
    void reset(){arm(0,{});}
    void fragment(Boundary b,uint64_t byte_offset){
        eligible_=token_&&b.after(after_);
        if(eligible_&&!boundary_set_){byte_boundary_=byte_offset;boundary_set_=true;}
    }
    void observe(const Parser::Message& m){
        if(m.cls!=5||(m.id!=0&&m.id!=1))return;
        if(m.size!=2){++malformed;return;}
        if(m.payload[0]!=6||m.payload[1]!=0x8a)return;
        if(m.id)++acks;else ++naks;
        if(eligible_&&boundary_set_&&m.offset>=byte_boundary_&&result_==Reply::None)
            result_=m.id?Reply::Ack:Reply::Nak;
    }
    Reply reply(uint32_t token)const{return token&&token==token_?result_:Reply::None;}
    uint32_t acks{},naks{},malformed{};
private:
    uint32_t token_{};Boundary after_{};uint64_t byte_boundary_{};
    bool eligible_{},boundary_set_{};Reply result_{Reply::None};
};
struct QualityCounters {
    uint32_t fix{},position{},speed{},accuracy{},utc{},speed_accuracy{};
    void observe(const Sample& s){
        auto r=s.position_rejections();fix+=bool(r&RejectFix);position+=bool(r&RejectPosition);
        speed+=bool(r&RejectSpeed);accuracy+=bool(r&RejectAccuracy);
        utc+=!s.utc_valid();speed_accuracy+=!s.sacc_mm_s||s.sacc_mm_s>3000;
    }
};
}
