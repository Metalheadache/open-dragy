// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#pragma once
#include <atomic>
#include <cstdint>
namespace dragy_ref {
struct Boundary {
    uint32_t session{},loss{},sequence{};
    bool same_stream(Boundary b)const{return session==b.session&&loss==b.loss;}
    bool after(Boundary b)const{return same_stream(b)&&int32_t(sequence-b.sequence)>0;}
};
// One control writer, one consumer; notification callbacks never touch this
// mailbox. Atomic fields and a bounded snapshot avoid torn configurations or
// waiting on a preempted writer. Revisions/sequence differences must be <2^31.
class BoundaryControl {
public:
    struct View {Boundary boundary{};uint32_t value{},revision{};};
    void publish(Boundary b,uint32_t value){
        ++revision_;session_=b.session;loss_=b.loss;sequence_=b.sequence;value_=value;++revision_;
    }
    bool read(View& out)const{
        const auto a=revision_.load();if(a&1)return false;
        View v{{session_.load(),loss_.load(),sequence_.load()},value_.load(),a};
        if(a!=revision_.load())return false;
        out=v;return true;
    }
    uint32_t revision()const{return revision_.load();}
private:
    std::atomic<uint32_t> revision_{0},session_{0},loss_{0},sequence_{0},value_{0};
};
}
