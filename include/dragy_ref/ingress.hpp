// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#pragma once
#include "dragy_ref/telemetry.hpp"
#include "dragy_ref/control.hpp"
#include <array>
#include <atomic>
#include <cstring>
namespace dragy_ref {
// Exactly one serialized notification producer and one consumer. Neither
// push nor pop allocates, waits for a lock, parses, logs or accesses storage.
// The connection owner may change the session atomically. Each subscription
// captures that token, so a delayed old callback cannot join a new session.
template<size_t Slots=32,size_t MaxBytes=512> class Ingress {
    static_assert(Slots>1 && MaxBytes<=65535);
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
public:
    struct Version {
        uint32_t session{}, loss{};
        bool operator==(Version b)const{return session==b.session&&loss==b.loss;}
        bool operator!=(Version b)const{return !(*this==b);}
    };
    struct Fragment {Version version{};uint32_t sequence{};uint64_t arrival_us{};uint16_t size{};std::array<uint8_t,MaxBytes> bytes{};};
    Boundary boundary()const{auto v=version();return {v.session,v.loss,sequence_.load()};}
    uint32_t new_session(){return session_.fetch_add(1,std::memory_order_acq_rel)+1;}
    Version version()const{return {session_.load(std::memory_order_acquire),loss_.load(std::memory_order_acquire)};}
    bool push(uint32_t session,const uint8_t* p,size_t n,uint64_t arrival) {
        if(session!=session_.load(std::memory_order_acquire))return false;
        if(!n)return true;
        uint32_t w=write_.load(std::memory_order_relaxed),next=(w+1)%Slots;
        if(!p||n>MaxBytes||next==read_.load(std::memory_order_acquire)) {
            drops_.fetch_add(1,std::memory_order_relaxed);
            loss_.fetch_add(1,std::memory_order_release);return false;
        }
        auto& f=slots_[w];f.sequence=++sequence_;f.version={session,loss_.load(std::memory_order_acquire)};
        f.arrival_us=arrival;f.size=uint16_t(n);std::memcpy(f.bytes.data(),p,n);
        write_.store(next,std::memory_order_release);return true;
    }
    bool pop(Fragment& out) {
        uint32_t r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire))return false;
        out=slots_[r];read_.store((r+1)%Slots,std::memory_order_release);return true;
    }
    uint32_t drops()const{return drops_.load(std::memory_order_relaxed);}
private:
    std::array<Fragment,Slots> slots_{};
    std::atomic<uint32_t> write_{0},read_{0},session_{0},loss_{0},drops_{0},sequence_{0};
};

// Consumer owns decoder and timing state. Hooks must be fast; hand off slow
// logging separately. Gap hook must interrupt active timing and reset filters.
template<class Queue> class Decoder {
public:
    using OnSample=void(*)(const Sample&,uint64_t,void*);
    using OnGap=void(*)(void*);
    using OnFragment=void(*)(Boundary,uint64_t,uint64_t,void*);
    using OnMessage=void(*)(const Parser::Message&,void*);
    Decoder(Queue& q,OnSample sample,OnGap gap,void* context,OnFragment fragment=nullptr,OnMessage message=nullptr):queue_(q),sample_(sample),gap_(gap),fragment_(fragment),message_(message),context_(context){}
    void drain() {
        sync();typename Queue::Fragment f;
        while(queue_.pop(f)) {
            sync();if(f.version!=active_)continue;
            arrival_=f.arrival_us;
            if(fragment_)fragment_({f.version.session,f.version.loss,f.sequence},arrival_,parser_.received(),context_);
            parser_.feed(f.bytes.data(),f.size,ready,this,observe);sync();
        }
    }
    const Parser& parser()const{return parser_;}
private:
    Queue& queue_;Parser parser_;typename Queue::Version active_{};
    OnSample sample_;OnGap gap_;OnFragment fragment_;OnMessage message_;void* context_;uint64_t arrival_{};
    void sync(){auto v=queue_.version();if(v!=active_){active_=v;parser_.reset();gap_(context_);}}
    static void observe(const Parser::Message& m,void* p){auto& d=*static_cast<Decoder*>(p);if(d.message_&&d.active_==d.queue_.version())d.message_(m,d.context_);}
    static void ready(const Sample& s,const uint8_t*,void* p) {
        auto& d=*static_cast<Decoder*>(p);
        if(d.active_==d.queue_.version())d.sample_(s,d.arrival_,d.context_);
    }
};
}
