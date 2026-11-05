// SPDX-License-Identifier: MPL-2.0 AND Apache-2.0
// Copyright 2026 Yizhan Wu (local contributions)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Retained upstream portions also carry Apache-2.0 terms; see NOTICE.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#pragma once
#include <array>
#include <cstdint>

// Shared by the ESP32 adapter and host fault tests. Successful ATT writes
// prove setup only; actual notifications and decoded samples prove the stream.
namespace dragy_ref::ble {
constexpr uint64_t warning_us=10000000, repair_us=30000000,
                   initial_grace_us=180000000, silence_us=10000000;
enum class Stage { Discover, Challenge, Response, Subscribe, Verify };
inline const char* name(Stage s){switch(s){
 case Stage::Discover:return "Discovering Dragy";case Stage::Challenge:return "Reading challenge";
 case Stage::Response:return "Unlocking Dragy";case Stage::Subscribe:return "Enabling GPS updates";
 case Stage::Verify:return "Verifying GPS updates";}return "BLE setup";}
template<class Adapter> bool setup(Adapter& io){
 io.stage(Stage::Discover);if(!io.discover())return io.fail("GATT discovery failed");
 if(!io.has_cccd())return io.fail("FD02 CCCD missing");
 io.stage(Stage::Challenge);std::array<uint8_t,2> challenge{};
 if(!io.read_challenge(challenge))return io.fail("FD03 read failed");
 const auto a=challenge[0],b=challenge[1];
 io.stage(Stage::Response);
 if(!io.write_response(std::array<uint8_t,4>{a,b,uint8_t(a^b),uint8_t(a&b)}))return io.fail("FD03 write failed");
 io.stage(Stage::Subscribe);if(!io.subscribe())return io.fail("FD02 subscribe failed");
 io.stage(Stage::Verify);uint16_t value=0;
 if(!io.read_cccd(value))return io.fail("FD02 CCCD read failed");
 if(value!=1)return io.fail("FD02 CCCD not enabled");
 return true;
}
enum class Stream { Waiting, Live, NoNotifications, Undecodable, Stale };
inline Stream stream(uint64_t now,uint64_t connected,uint64_t notification,uint64_t sample){
 if(sample&&now>=sample&&now-sample<1000000)return Stream::Live;
 if(now<connected||now-connected<warning_us)return sample?Stream::Stale:Stream::Waiting;
 if(!notification||now<notification||now-notification>silence_us)return Stream::NoNotifications;
 if(!sample)return Stream::Undecodable;
 return Stream::Stale;
}
inline unsigned backoff(unsigned failures){constexpr unsigned seconds[]={1,2,5,10};return seconds[failures<4?failures:3];}
inline bool repair_due(uint64_t now,uint64_t connected,bool seen_sample,bool repaired){return !seen_sample&&!repaired&&now>=connected&&now-connected>=repair_us;}
inline bool reconnect_due(uint64_t now,uint64_t connected,uint64_t notification,uint64_t sample,bool seen_sample){
 if(!seen_sample)return now>=connected&&now-connected>=initial_grace_us;
 return !notification||!sample||(now>=notification&&now-notification>=silence_us)||(now>=sample&&now-sample>=silence_us);
}
}
