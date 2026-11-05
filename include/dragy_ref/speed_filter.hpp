// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Yizhan Wu
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Modified 2026-11-05: extracted and reduced to the standalone reference scope.
#pragma once
#include "dragy_ref/telemetry.hpp"
namespace dragy_ref {
// Presentation-only stationary speed hold.
// Never edits Sample or feeds the timing engine.
class DisplaySpeedHold {
public:
    void accept(const Sample&,uint64_t arrival_us);
    void reset() { *this=DisplaySpeedHold{}; }
    int32_t speed_mm_s(uint64_t now_us) const {
        return stationary_ && fresh(now_us)?0:raw_mm_s_;
    }
    bool stationary(uint64_t now_us) const { return stationary_&&fresh(now_us); }
    bool candidate(uint64_t now_us) const { return candidate_&&fresh(now_us); }
private:
    bool fresh(uint64_t now) const { return seen_&&now>=arrival_us_&&now-arrival_us_<=500000; }
    void clear_hold() { stationary_=candidate_=leaving_=false; }
    Timebase time_;
    uint64_t arrival_us_{},candidate_ms_{},leaving_ms_{},candidate_arrival_us_{},leaving_arrival_us_{};
    int32_t raw_mm_s_{};
    bool seen_{},stationary_{},candidate_{},leaving_{};
};
}
