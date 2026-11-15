// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#pragma once
#include "dragy_ref/rate.hpp"
#include "dragy_ref/protocol.hpp"
namespace dragy_ref::ble {
enum class Completion {Pending,Success,Failure};
// IO is the ESP adapter or a deterministic fake. Async callbacks publish only
// token-tagged results. This controller is owned/polled by the connection task.
// Optional ATT operations never overlap, including after a completion timeout.
template<class IO> class RateSetup {
public:
    enum class Stage {Idle,Model,Observe,Write,Firmware};
    explicit RateSetup(IO& io):io_(io){}
    void cancel(){if(att_owned())blocked_=true;io_.invalidate();io_.watch(0);io_.gate(true);stage_=Stage::Idle;watching_=false;}
    void start(uint64_t now,Receiver advertised,bool enabled,bool provisional=false,bool new_connection=false){
        cancel();if(new_connection)blocked_=false;
        if(blocked_){io_.gate(false);io_.log("optional ATT remains blocked until a new connection");return;}
        hint_=advertised;enabled_=enabled;provisional_=provisional;actual_=Receiver::Unknown;
        token_=io_.next_token();started_=now;stage_=Stage::Model;
        if(!io_.model(token_)){io_.gate(false);firmware(now);}
    }
    void poll(uint64_t now){
        if(stage_==Stage::Observe){
            if(now>=started_&&now-started_>=750000){
                if(io_.clean25(now)){io_.log("existing measured 25 Hz; rate write skipped");io_.gate(false);firmware(now);}
                else write(now);
            }
        }else if(stage_!=Stage::Idle){
            if(now>=started_&&now-started_>=3000000){
                const bool was_firmware=stage_==Stage::Firmware;
                blocked_=true;io_.invalidate();stage_=Stage::Idle;io_.log("optional ATT timeout; no overlapping procedure");
                if(!was_firmware){watching_=false;io_.watch(0);io_.gate(false);}
            }else{
                auto done=io_.completion(token_);
                if(done!=Completion::Pending){
                    if(stage_==Stage::Model){
                        actual_=io_.model_receiver();const auto profile=rate_receiver(actual_,hint_);
                        if(done==Completion::Success&&enabled_&&request_25hz(profile)&&io_.writable()&&
                           (profile==Receiver::Pro||provisional_)){
                            io_.profile(profile,actual_==Receiver::Unknown);stage_=Stage::Observe;started_=now;
                        }else {io_.gate(false);firmware(now);}
                    }else if(stage_==Stage::Write){
                        watching_=done==Completion::Success;
                        io_.log(watching_?"rate write ATT accepted; reply and cadence independent":"rate write failed; stream retained");
                        io_.gate(false);if(!watching_)io_.watch(0);firmware(now);
                    }else stage_=Stage::Idle;
                }
            }
        }
        if(watching_){
            const auto reply=io_.reply(request_);
            if(reply!=Reply::None&&!reply_logged_){reply_logged_=true;io_.log(reply==Reply::Ack?"receiver CFG-VALSET ACK":"receiver CFG-VALSET NAK");}
            if(reply==Reply::Nak){watching_=false;io_.watch(0);}
            else if(io_.timing_ready(now)){watching_=false;io_.log("fresh receiver epochs confirmed 25 Hz");io_.watch(0);}
            else if(now>=requested_&&now-requested_>=10000000){watching_=false;io_.log("25 Hz unconfirmed; timing unavailable");io_.watch(0);}
        }
    }
    bool att_owned()const{return blocked_||stage_==Stage::Model||stage_==Stage::Write||stage_==Stage::Firmware;}
    Stage stage()const{return stage_;}
    bool watching()const{return watching_;}
private:
    IO& io_;Stage stage_{Stage::Idle};Receiver hint_{},actual_{};
    uint32_t token_{},request_{};uint64_t started_{},requested_{};
    bool enabled_{},provisional_{},watching_{},reply_logged_{},blocked_{};
    void firmware(uint64_t now){
        token_=io_.next_token();started_=now;stage_=Stage::Firmware;
        if(!io_.firmware(token_))stage_=Stage::Idle;
    }
    void write(uint64_t now){
        token_=io_.next_token();request_=token_;started_=requested_=now;stage_=Stage::Write;reply_logged_=false;
        io_.gate(true);io_.watch(request_);
        if(!io_.write(token_,pro_rate_frame())){io_.watch(0);io_.gate(false);firmware(now);}
    }
};
}
