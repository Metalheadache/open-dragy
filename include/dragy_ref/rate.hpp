// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
// Modified 2026-11-15: bounded protocol diagnostics and receiver setup update.
#pragma once
#include "dragy_ref/telemetry.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include <string_view>
namespace dragy_ref::ble {
enum class Receiver : uint8_t { Unknown, Pro, Drg70, Lite, Legacy };
inline char ascii_lower(char c){return c>='A'&&c<='Z'?char(c+'a'-'A'):c;}
inline bool istarts(std::string_view text,std::string_view prefix){
 if(text.size()<prefix.size())return false;
 for(size_t i=0;i<prefix.size();++i)if(ascii_lower(text[i])!=ascii_lower(prefix[i]))return false;
 return true;
}
inline bool icontains(std::string_view text,std::string_view part){
 for(size_t i=0;i+part.size()<=text.size();++i)if(istarts(text.substr(i),part))return true;
 return false;
}
inline bool dragy_candidate(std::string_view name,bool fd00){
 return fd00||istarts(name,"drg")||icontains(name,"dragy");
}
inline bool model_family(std::string_view text,std::string_view family){
 return istarts(text,family)&&(text.size()==family.size()||text[family.size()]==' '||text[family.size()]=='-');
}
inline Receiver receiver_model(std::string_view text){
 text=text.substr(0,text.find('\0'));
 while(!text.empty()&&text.back()==' ')text.remove_suffix(1);
 if(model_family(text,"DRG71"))return Receiver::Pro;
 if(model_family(text,"DRG70")||model_family(text,"DRG70C"))return Receiver::Drg70;
 if(model_family(text,"DRG69"))return Receiver::Legacy;
 if(model_family(text,"Lite")||model_family(text,"Dragy Lite")||model_family(text,"Dragy-Lite")||
    model_family(text,"Dragy_Lite")||model_family(text,"DragyLite")||model_family(text,"DRG Lite")||
    model_family(text,"DRGLite"))return Receiver::Lite;
 return Receiver::Unknown;
}
inline Receiver rate_receiver(Receiver model,Receiver advertised){
 if(model!=Receiver::Unknown)return model;
 // Lite's numeric model ID is not assumed. A same-peer product-name hint can
 // enable the provisional request after the common Dragy GATT handshake.
 return advertised==Receiver::Drg70||advertised==Receiver::Lite?advertised:Receiver::Unknown;
}
inline bool request_25hz(Receiver r){return r==Receiver::Pro||r==Receiver::Drg70||r==Receiver::Lite;}
inline const char* receiver_name(Receiver r){switch(r){
 case Receiver::Pro:return "DRG71";case Receiver::Drg70:return "DRG70";case Receiver::Lite:return "Dragy Lite";
 case Receiver::Legacy:return "DRG69";default:return "unknown";}}
// u-blox M10 CFG-VALSET: only CFG-RATE-MEAS=40 ms, RAM layer only.
// DRG71 FD01 transport has a prior physical baseline. This reference update
// has software/build validation only; it is not new receiver acceptance.
// DRG70/Lite use this common-interface request provisionally; physical tests pending.
inline std::array<uint8_t,18> pro_rate_frame(){
 std::array<uint8_t,18> b{0xb5,0x62,0x06,0x8a,10,0,0,1,0,0,1,0,0x21,0x30,40,0,0,0};
 for(size_t i=2;i<16;i++){b[16]+=b[i];b[17]+=b[16];}return b;
}
inline bool pro_model(const uint8_t* p,size_t n){return n>=5&&p[0]=='D'&&p[1]=='R'&&p[2]=='G'&&p[3]=='7'&&p[4]=='1'&&(n==5||p[5]==0||p[5]==' '||p[5]=='-');}
// Require 25 consecutive forward 40 ms intervals after the request. ATT
// success or callback frequency alone cannot establish receiver cadence.
class CadenceVerifier {
    Timebase time_;unsigned consecutive_{};
public:
    void reset(){time_.reset();consecutive_=0;}
    bool observe(uint32_t tow){
        auto e=time_.accept(tow);
        if(e==Epoch::Duplicate||e==Epoch::Backward)return confirmed();
        consecutive_=(e==Epoch::Forward&&time_.delta_ms==40)?consecutive_+unsigned(consecutive_<25):0;
        return confirmed();
    }
    bool confirmed()const{return consecutive_>=25;}
};
}
