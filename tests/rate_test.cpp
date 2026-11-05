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
 for(auto name:{"DRG70","DRG69","DRG710","DRG71X","DRG7","UNKNOWN"})assert(!dragy_ref::ble::pro_model(reinterpret_cast<const uint8_t*>(name),strlen(name)));
 puts("PASS GPS rate: independently checked RAM-only CFG-RATE-MEAS 40ms bytes/checksum and strict DRG71 model guard");
}
