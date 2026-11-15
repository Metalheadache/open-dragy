// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#pragma once
#include "dragy_ref/drag.hpp"
#include "dragy_ref/timing_bounds.hpp"
namespace dragy_ref::drag {
// Optional strict-25-Hz integration. The ordinary Engine retains its original
// 5/10/25 Hz policy. Only this gate can deliver a confirmed gap to that engine.
class TimingGate25 {
public:
    static constexpr unsigned required_intervals=8;
    void arm(){engine_.arm();clear();}
    void disarm(){engine_.disarm();clear();}
    void interrupt(Fault why=Fault::Interrupted){engine_.interrupt(why);clear();}
    void configure(bool pending){interrupt();pending_=pending;}
    void accept(const Sample& s,uint64_t arrival,uint32_t errors=0,bool after_boundary=true){
        const auto e=time_.accept(s.tow_ms);const auto dt=time_.delta_ms;
        const bool fresh=!last_arrival_||(arrival>=last_arrival_&&arrival-last_arrival_<=250000);
        if(!after_boundary){break_timing();clean_=0;errors_=errors;return;}
        if(e==Epoch::Duplicate){clean_=0;return;} // duplicates do not refresh readiness
        if(e==Epoch::Backward||e==Epoch::Reset||e==Epoch::Invalid){break_timing();clean_=0;errors_=errors;return;}
        if(!fresh){clean_=0;if(last_arrival_&&(arrival<last_arrival_||arrival-last_arrival_>500000))break_timing();}
        clean_=(fresh&&e==Epoch::Forward&&dt==40&&errors==errors_)?std::min(clean_+1,1000u):0;
        errors_=errors;last_arrival_=arrival;
        if(pending_)break_timing();
        else if(!qualified_){
            if(clean_>=required_intervals){qualified_=true;ordinary_=required_intervals;engine_.accept(s,arrival);}
        }else if(e==Epoch::Forward&&dt==40){
            if(held_){
                // The engine may complete a held finish before discovering a
                // confirming speed jump/latency fault. Keep its public results
                // transactional until both original samples pass all checks.
                const View before=engine_.view_;const auto faults=engine_.discontinuities_;
                engine_.accept_impl(held_sample_,held_arrival_,true);engine_.accept(s,arrival);
                if(engine_.discontinuities_==faults){++bridged;ordinary_=1;}
                else {
                    const auto reason=engine_.last_rejection_;engine_.view_=before;
                    engine_.interrupt(reason);++rejected;qualified_=false;ordinary_=clean_=0;
                }
                held_=false;previous_=s;return;
            }
            ordinary_=std::min(ordinary_+1,required_intervals);engine_.accept(s,arrival);
        }else if(!held_&&ordinary_>=required_intervals&&e==Epoch::Forward&&dt>40&&
                 quality(previous_)&&quality(s)&&timing::short_gap(previous_,s,dt)){
            held_sample_=s;held_arrival_=arrival;held_=true;ordinary_=0;
        }else {++rejected;break_timing();clean_=0;}
        previous_=s;
    }
    void poll(uint64_t now){
        engine_.poll(now);
        if(last_arrival_&&(now<last_arrival_||now-last_arrival_>500000)){break_timing();clean_=0;}
    }
    bool ready(uint64_t now)const{return !pending_&&qualified_&&!held_&&last_arrival_&&now>=last_arrival_&&now-last_arrival_<=250000;}
    unsigned clean_intervals()const{return clean_;}
    bool awaiting_confirmation()const{return held_;}
    const View& view()const{return engine_.view();}
    uint32_t bridged{},rejected{};
private:
    Engine engine_;Timebase time_;Sample previous_{},held_sample_{};
    uint64_t last_arrival_{},held_arrival_{};uint32_t errors_{};
    unsigned clean_{},ordinary_{};bool pending_{true},qualified_{},held_{};
    static bool quality(const Sample& s){return s.position_valid()&&s.fix==3&&s.satellites>=6&&s.hacc_mm<=5000&&s.sacc_mm_s&&s.sacc_mm_s<=500;}
    void break_timing(){if(qualified_)engine_.interrupt();qualified_=false;held_=false;ordinary_=0;}
    void clear(){time_.reset();last_arrival_=0;clean_=ordinary_=0;qualified_=held_=false;errors_=0;}
};
}
