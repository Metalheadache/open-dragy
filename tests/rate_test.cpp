// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#include "dragy_ref/rate.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
 constexpr std::array<uint8_t,18> expected{0xb5,0x62,0x06,0x8a,0x0a,0,0,1,0,0,1,0,0x21,0x30,0x28,0,0x15,0x41};
 assert(dragy_ref::ble::pro_rate_frame()==expected);assert(!dragy_ref::ble::pro_model(nullptr,0));
 for(auto name:{"DRG71","DRG71 ","DRG71-1"})assert(dragy_ref::ble::pro_model(reinterpret_cast<const uint8_t*>(name),strlen(name)));
 for(auto name:{"DRG70","DRG70-C","Dragy Lite","UNVERIFIED","DRG69","DRG710","DRG71X","DRG7","UNKNOWN"})assert(!dragy_ref::ble::pro_model(reinterpret_cast<const uint8_t*>(name),strlen(name)));
 using namespace dragy_ref::ble;
 for(auto name:{"DRG70","DRG70-C","DRG70C","drg70","DRG70-1"})assert(receiver_model(name)==Receiver::Drg70);
 for(auto name:{"Lite","Dragy Lite","dragy lite","Dragy-Lite","Dragy_Lite","DragyLite","DRG Lite","DRGLite"})assert(receiver_model(name)==Receiver::Lite);
 for(auto name:{"DRG700","DRG70X","DRG710","DRG71X","DRG72","LiteX","UNKNOWN"})assert(receiver_model(name)==Receiver::Unknown);
 for(auto name:{"DRG71","DRG71 ","DRG71-1"})assert(receiver_model(name)==Receiver::Pro);
 assert(receiver_model(std::string_view("DRG70\0junk",10))==Receiver::Drg70);
 assert(receiver_model({})==Receiver::Unknown);
 for(auto name:{"DRG70","drg70","DRG70-C","dragy lite","DRG Lite","my DRAGY receiver"})assert(dragy_candidate(name,false));
 assert(dragy_candidate("",true)&&dragy_candidate("Unlabelled GPS",true));
 assert(!dragy_candidate("Lite",false)&&!dragy_candidate("Headphones",false));
 assert(rate_receiver(Receiver::Unknown,Receiver::Lite)==Receiver::Lite);
 assert(rate_receiver(Receiver::Unknown,Receiver::Drg70)==Receiver::Drg70);
 assert(rate_receiver(Receiver::Unknown,Receiver::Pro)==Receiver::Unknown);
 assert(rate_receiver(Receiver::Legacy,Receiver::Lite)==Receiver::Legacy);
 assert(!request_25hz(Receiver::Unknown)&&!request_25hz(Receiver::Legacy));
 assert(request_25hz(Receiver::Pro)&&request_25hz(Receiver::Drg70)&&request_25hz(Receiver::Lite));
 puts("PASS GPS profiles/discovery: case-insensitive names, strict model families, legacy/unknown exclusions, advertisement fallback and unchanged RAM-only 40ms frame");
}
